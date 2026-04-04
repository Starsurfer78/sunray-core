import 'dart:async';
import 'dart:convert';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/config/app_constants.dart';
import '../../domain/robot/robot_status.dart';
import '../../domain/robot/saved_robot.dart';
import '../../domain/mission/mission.dart';
import '../../shared/providers/app_providers.dart';
import 'robot_repository.dart';
import 'robot_ws.dart';

class RobotConnectionController {
  RobotConnectionController({
    required Ref ref,
    required RobotRepository repository,
    required RobotWsClient ws,
  })  : _ref = ref,
        _repository = repository,
        _ws = ws;

  static const Duration _connectTimeout = Duration(seconds: 8);

  final Ref _ref;
  final RobotRepository _repository;
  final RobotWsClient _ws;

  StreamSubscription<dynamic>? _telemetrySubscription;
  Timer? _reconnectTimer;
  SavedRobot? _connectedRobot;
  bool _manualDisconnect = false;
  int _reconnectAttempt = 0;

  Future<RobotStatus> connect(
    SavedRobot robot, {
    bool persist = true,
  }) async {
    _manualDisconnect = false;
    _connectedRobot = robot;
    _reconnectTimer?.cancel();
    _ref.read(connectionStateProvider.notifier).state = RobotStatus(
      connectionState: ConnectionStateKind.connecting,
      robotName: robot.name,
    );

    final resolvedConnection = await _probeRobotWithFallback(robot).timeout(
      _connectTimeout,
      onTimeout: () => _ResolvedConnection(
        robot: robot,
        status: RobotStatus(
          connectionState: ConnectionStateKind.error,
          robotName: robot.name,
          lastError: 'Verbindungstimeout nach ${_connectTimeout.inSeconds}s',
        ),
      ),
    );
    final resolvedRobot = resolvedConnection.robot;
    final status = resolvedConnection.status;
    _connectedRobot = resolvedRobot;

    if (status.connectionState != ConnectionStateKind.connected) {
      _ref.read(connectionStateProvider.notifier).state = status;
      return status;
    }

    try {
      final mapGeometry = await _repository
          .fetchMapGeometry(
            host: resolvedRobot.lastHost,
            port: resolvedRobot.port,
          )
          .timeout(
            _connectTimeout,
            onTimeout: () => throw Exception(
              'Map-Timeout nach ${_connectTimeout.inSeconds}s',
            ),
          );

      final missions = await _loadMissions(resolvedRobot);

      if (persist) {
        await _ref.read(robotStorageProvider).saveConnectedRobot(resolvedRobot);
        _ref.invalidate(savedRobotsProvider);
        _ref.invalidate(rememberedRobotProvider);
      }

      _ref.read(activeRobotProvider.notifier).state = resolvedRobot;
      _ref.read(mapGeometryProvider.notifier).state = mapGeometry;
      _ref.read(missionsProvider.notifier).state = missions;
      _ref.read(selectedMissionIdProvider.notifier).state =
          missions.isEmpty ? null : missions.first.id;
      await _ref.read(appStateStorageProvider).saveMapGeometry(mapGeometry);
      await _ref.read(appStateStorageProvider).saveMissions(missions);
      _ref.read(connectionStateProvider.notifier).state = status;

      _reconnectAttempt = 0;
      await _bindTelemetry(resolvedRobot, status);
      return _ref.read(connectionStateProvider);
    } catch (error) {
      final failed = RobotStatus(
        connectionState: ConnectionStateKind.error,
        robotName: robot.name,
        lastError: 'Verbindung fehlgeschlagen: $error',
      );
      _ref.read(connectionStateProvider.notifier).state = failed;
      return failed;
    }
  }

  Future<_ResolvedConnection> _probeRobotWithFallback(SavedRobot robot) async {
    RobotStatus? lastStatus;

    for (final port in _candidatePorts(robot.port)) {
      final status = await _repository.probeRobot(
        host: robot.lastHost,
        port: port,
        fallbackName: robot.name,
      );
      if (status.connectionState == ConnectionStateKind.connected) {
        return _ResolvedConnection(
          robot: SavedRobot(
            id: '${robot.lastHost}:$port',
            name: robot.name,
            lastHost: robot.lastHost,
            port: port,
            lastSeen: DateTime.now(),
          ),
          status: status,
        );
      }

      lastStatus = status;
      if (!_looksLikeConnectionRefused(status.lastError)) {
        break;
      }
    }

    return _ResolvedConnection(
      robot: robot,
      status: lastStatus ??
          RobotStatus(
            connectionState: ConnectionStateKind.error,
            robotName: robot.name,
            lastError: 'Verbindung fehlgeschlagen',
          ),
    );
  }

  Iterable<int> _candidatePorts(int requestedPort) sync* {
    final seen = <int>{};
    if (seen.add(requestedPort)) yield requestedPort;
    for (final port in AppConstants.fallbackRobotPorts) {
      if (seen.add(port)) yield port;
    }
  }

  bool _looksLikeConnectionRefused(String? error) {
    if (error == null) return false;
    final normalized = error.toLowerCase();
    return normalized.contains('connection refused') ||
        normalized.contains('connectionfailed') ||
        normalized.contains('socketexception') ||
        normalized.contains('errno = 111') ||
        normalized.contains('os error: connection refused');
  }

  Future<void> disconnect() async {
    _manualDisconnect = true;
    _reconnectTimer?.cancel();
    await _telemetrySubscription?.cancel();
    _telemetrySubscription = null;
    await _ws.disconnect();
    _connectedRobot = null;
    _ref.read(activeRobotProvider.notifier).state = null;
    _ref.read(connectionStateProvider.notifier).state = const RobotStatus(
      connectionState: ConnectionStateKind.disconnected,
    );
  }

  void stopReconnect() {
    _manualDisconnect = true;
    _reconnectTimer?.cancel();
  }

  Future<void> switchRobot() async {
    await disconnect();
    await _ref.read(robotStorageProvider).clearActiveRobot();
    _ref.invalidate(savedRobotsProvider);
    _ref.invalidate(rememberedRobotProvider);
  }

  void startMowing({String? missionId}) {
    if (!_ws.isConnected) return;
    if (missionId != null && missionId.trim().isNotEmpty) {
      _ws.sendCommand('start', <String, dynamic>{'missionId': missionId});
      return;
    }
    _ws.sendCommand('start');
  }

  void emergencyStop() {
    if (!_ws.isConnected) return;
    _ws.sendCommand('stop');
  }

  void startDocking() {
    if (!_ws.isConnected) return;
    _ws.sendCommand('dock');
  }

  /// [linear] = forward/back (-1..1), [angular] = left/right turn (-1..1, negative = right).
  void sendDriveCommand(double linear, double angular) {
    if (!_ws.isConnected) return;
    _ws.sendCommand('drive', <String, dynamic>{'linear': linear, 'angular': angular});
  }

  Future<List<Mission>> _loadMissions(SavedRobot robot) async {
    _ref.read(missionsLoadingProvider.notifier).state = true;
    try {
      final missions = await _ref
          .read(missionRepositoryProvider)
          .fetchMissions(host: robot.lastHost, port: robot.port)
          .timeout(_connectTimeout, onTimeout: () => const <Mission>[]);
      if (missions.isNotEmpty) return missions;
      return _ref.read(missionsProvider);
    } catch (_) {
      return _ref.read(missionsProvider);
    } finally {
      _ref.read(missionsLoadingProvider.notifier).state = false;
    }
  }

  Future<void> _bindTelemetry(
    SavedRobot robot,
    RobotStatus baseStatus,
  ) async {
    await _telemetrySubscription?.cancel();
    await _ws.disconnect();

    final stream = _ws.connect(host: robot.lastHost, port: robot.port);
    if (stream == null) return;

    _telemetrySubscription = stream.listen(
      (event) => _handleTelemetryEvent(event, baseStatus, robot),
      onError: (Object error, StackTrace stackTrace) {
        _ref.read(connectionStateProvider.notifier).state = RobotStatus(
          connectionState: ConnectionStateKind.error,
          robotName: robot.name,
          lastError: 'WebSocket getrennt: $error',
        );
        _scheduleReconnect();
      },
      onDone: () {
        final current = _ref.read(connectionStateProvider);
        if (current.connectionState == ConnectionStateKind.connected) {
          _ref.read(connectionStateProvider.notifier).state = RobotStatus(
            connectionState: ConnectionStateKind.disconnected,
            robotName: robot.name,
            batteryPercent: current.batteryPercent,
            rtkState: current.rtkState,
            statePhase: current.statePhase,
          );
        }
        _scheduleReconnect();
      },
      cancelOnError: true,
    );
  }

  void _handleTelemetryEvent(
    dynamic event,
    RobotStatus baseStatus,
    SavedRobot robot,
  ) {
    try {
      final dynamic decoded = event is String ? jsonDecode(event) : event;
      if (decoded is! Map<String, dynamic>) return;
      if (decoded['type'] != 'state') return;

      final gpsSol = decoded['gps_sol'] as num?;
      final batteryVoltage = decoded['battery_v'] as num?;
      final gpsText = decoded['gps_text'] as String?;
      final statePhase = decoded['state_phase'] as String?;
      final x = decoded['x'] as num?;
      final y = decoded['y'] as num?;
      final heading = decoded['heading'] as num?;
      final gpsLat = decoded['gps_lat'] as num?;
      final gpsLon = decoded['gps_lon'] as num?;
      final piVersion = decoded['pi_v'] as String?;

      _ref.read(connectionStateProvider.notifier).state = RobotStatus(
        connectionState: ConnectionStateKind.connected,
        robotName: baseStatus.robotName ?? robot.name,
        batteryPercent: _batteryPercentFromVoltage(batteryVoltage?.toDouble()),
        rtkState: _gpsStateFromTelemetry(gpsSol?.toInt(), gpsText),
        statePhase: statePhase,
        x: x?.toDouble(),
        y: y?.toDouble(),
        heading: heading?.toDouble(),
        gpsLat: gpsLat?.toDouble(),
        gpsLon: gpsLon?.toDouble(),
        piVersion: piVersion ?? baseStatus.piVersion,
      );
      _reconnectAttempt = 0;
    } catch (_) {
      // Ignore malformed telemetry packets to keep the session stable.
    }
  }

  void _scheduleReconnect() {
    if (_manualDisconnect) return;
    final robot = _connectedRobot ?? _ref.read(activeRobotProvider);
    if (robot == null) return;

    _reconnectTimer?.cancel();
    final seconds = 1 << _reconnectAttempt.clamp(0, 4);
    final delay = Duration(seconds: seconds);
    _reconnectAttempt += 1;

    final current = _ref.read(connectionStateProvider);
    _ref.read(connectionStateProvider.notifier).state = current.copyWith(
      connectionState: ConnectionStateKind.connecting,
      robotName: robot.name,
      lastError: 'Verbindung verloren, neuer Versuch in ${delay.inSeconds}s',
    );

    _reconnectTimer = Timer(delay, () async {
      if (_manualDisconnect) return;
      await connect(robot, persist: false);
    });
  }

  int? _batteryPercentFromVoltage(double? batteryVoltage) {
    if (batteryVoltage == null || batteryVoltage <= 0) return null;
    final raw = ((batteryVoltage - 22.0) / (29.4 - 22.0)) * 100.0;
    return raw.clamp(0.0, 100.0).round();
  }

  String _gpsStateFromTelemetry(int? gpsSol, String? gpsText) {
    if (gpsSol == 4) return 'RTK Fix';
    if (gpsSol == 5) return 'RTK Float';
    if (gpsText != null && gpsText.trim().isNotEmpty) return gpsText.trim();
    return 'GPS unbekannt';
  }
}

class _ResolvedConnection {
  const _ResolvedConnection({
    required this.robot,
    required this.status,
  });

  final SavedRobot robot;
  final RobotStatus status;
}
