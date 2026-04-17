import 'dart:async';
import 'dart:convert';

import '../../core/config/app_constants.dart';
import '../../domain/map/map_geometry.dart';
import '../../domain/map/stored_map.dart';
import '../../domain/mission/mission.dart';
import '../../domain/robot/robot_status.dart';
import '../../domain/robot/saved_robot.dart';
import 'robot_repository.dart';
import 'robot_ws.dart';

abstract class RobotStateSink {
  void applyConnectedRobot(SavedRobot? robot, {bool notify = true});
  void applyConnectionStatus(RobotStatus status, {bool notify = true});
  void applyMapGeometry(MapGeometry geometry, {bool notify = true});
  void applyMissions(List<Mission> missions, {bool notify = true});
  void applyStoredMaps(
    String activeId,
    List<StoredMap> maps, {
    bool notify = true,
  });
  void applyTelemetry(RobotStatus status);
}

class RobotService {
  RobotService({
    required RobotStateSink stateSink,
    required RobotRepository repository,
  }) : _stateSink = stateSink,
       _repository = repository,
       _ws = repository.ws;

  static const Duration _connectTimeout = Duration(seconds: 8);
  static const Duration _reconnectProbeTimeout = Duration(seconds: 4);
  static const Duration _watchdogTimeout = Duration(seconds: 10);

  final RobotStateSink _stateSink;
  final RobotRepository _repository;
  final RobotWsClient _ws;

  StreamSubscription<dynamic>? _telemetrySubscription;
  Timer? _reconnectTimer;
  Timer? _telemetryWatchdog;
  SavedRobot? _connectedRobot;
  bool _manualDisconnect = false;
  int _reconnectAttempt = 0;

  Future<RobotStatus> connect(SavedRobot robot) async {
    _manualDisconnect = false;
    _connectedRobot = robot;
    _reconnectTimer?.cancel();
    _stateSink.applyConnectionStatus(
      RobotStatus(
        connectionState: ConnectionStateKind.connecting,
        robotName: robot.name,
      ),
      notify: true,
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
      _stateSink.applyConnectionStatus(status);
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

      _stateSink.applyConnectedRobot(resolvedRobot, notify: false);
      _stateSink.applyMapGeometry(mapGeometry, notify: false);
      _stateSink.applyMissions(missions, notify: false);

      try {
        final storedResult = await _repository.fetchStoredMaps(
          host: resolvedRobot.lastHost,
          port: resolvedRobot.port,
        );
        _stateSink.applyStoredMaps(
          storedResult.activeId,
          storedResult.maps,
          notify: false,
        );
      } catch (_) {
        // Non-fatal: stored maps list supplements the live map.
        _stateSink.applyStoredMaps('', const <StoredMap>[], notify: false);
      }

      _stateSink.applyConnectionStatus(status);

      _reconnectAttempt = 0;
      await _bindTelemetry(resolvedRobot, status);
      return status;
    } catch (error) {
      final failed = RobotStatus(
        connectionState: ConnectionStateKind.error,
        robotName: robot.name,
        lastError: 'Verbindung fehlgeschlagen: $error',
      );
      _stateSink.applyConnectionStatus(failed);
      return failed;
    }
  }

  Future<void> disconnect({bool clearKnownRobot = false}) async {
    final previousRobot = _connectedRobot;
    _manualDisconnect = true;
    _reconnectTimer?.cancel();
    _telemetryWatchdog?.cancel();
    _telemetryWatchdog = null;
    await _telemetrySubscription?.cancel();
    _telemetrySubscription = null;
    await _ws.disconnect();
    _connectedRobot = null;
    _stateSink.applyConnectedRobot(
      clearKnownRobot ? null : previousRobot,
      notify: false,
    );
    _stateSink.applyConnectionStatus(
      const RobotStatus(connectionState: ConnectionStateKind.disconnected),
    );
  }

  Future<void> dispose() async {
    await disconnect(clearKnownRobot: false);
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

  void sendDriveCommand(double linear, double angular) {
    if (!_ws.isConnected) return;
    _ws.sendCommand('drive', <String, dynamic>{
      'linear': linear.clamp(-1.0, 1.0),
      'angular': angular.clamp(-1.0, 1.0),
    });
  }

  Future<void> setMowMotor(bool on) async {
    final robot = _connectedRobot;
    if (robot == null) return;
    await _repository.api.runDiagnostic(
      host: robot.lastHost,
      port: robot.port,
      action: 'mow',
      payload: <String, dynamic>{'on': on},
    );
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
      status:
          lastStatus ??
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

  Future<List<Mission>> _loadMissions(SavedRobot robot) async {
    final raw = await _repository.api
        .getMissions(host: robot.lastHost, port: robot.port)
        .timeout(_connectTimeout, onTimeout: () => const <dynamic>[]);
    return raw
        .whereType<Map<String, dynamic>>()
        .map(_parseMission)
        .where((mission) => mission.id.isNotEmpty)
        .toList(growable: false);
  }

  Mission _parseMission(Map<String, dynamic> raw) {
    final zoneIds = (raw['zoneIds'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<String>()
        .toList(growable: false);
    final zoneNames = (raw['zoneNames'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<String>()
        .toList(growable: false);
    final schedule = raw['schedule'];
    final scheduleLabel =
        raw['scheduleLabel'] as String? ??
        (schedule is Map<String, dynamic>
            ? _scheduleLabelFromMap(schedule)
            : null);

    return Mission(
      id: raw['id'] as String? ?? '',
      name: raw['name'] as String? ?? 'Mission',
      zoneIds: zoneIds,
      zoneNames: zoneNames,
      scheduleDays:
          (raw['scheduleDays'] as List<dynamic>? ?? const <dynamic>[]).length ==
              7
          ? (raw['scheduleDays'] as List<dynamic>)
                .map((entry) => entry == true)
                .toList(growable: false)
          : const <bool>[false, false, false, false, false, false, false],
      scheduleHour: raw['scheduleHour'] as int?,
      scheduleMinute: raw['scheduleMinute'] as int?,
      scheduleEndHour: raw['scheduleEndHour'] as int?,
      scheduleEndMinute: raw['scheduleEndMinute'] as int?,
      scheduleLabel: scheduleLabel,
      isRecurring: raw['isRecurring'] as bool? ?? false,
      onlyWhenDry: raw['onlyWhenDry'] as bool? ?? true,
      requiresHighBattery: raw['requiresHighBattery'] as bool? ?? false,
      pattern: raw['pattern'] as String? ?? 'Streifen',
    );
  }

  String? _scheduleLabelFromMap(Map<String, dynamic> raw) {
    final time = raw['time'] as String?;
    final days = (raw['days'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<String>()
        .toList(growable: false);
    if (days.isEmpty && (time == null || time.isEmpty)) return null;
    if (days.isEmpty) return time;
    return '${days.join(' ')} ${time ?? ''}'.trim();
  }

  Future<void> _bindTelemetry(SavedRobot robot, RobotStatus baseStatus) async {
    _telemetryWatchdog?.cancel();
    _telemetryWatchdog = null;
    await _telemetrySubscription?.cancel();
    await _ws.disconnect();

    final stream = _ws.connect(host: robot.lastHost, port: robot.port);
    if (stream == null) return;

    _telemetrySubscription = stream.listen(
      (event) => _handleTelemetryEvent(event, baseStatus, robot),
      onError: (Object error, StackTrace stackTrace) {
        _stateSink.applyConnectionStatus(
          RobotStatus(
            connectionState: ConnectionStateKind.error,
            robotName: robot.name,
            lastError: 'WebSocket getrennt: $error',
          ),
        );
        _scheduleReconnect();
      },
      onDone: () {
        _stateSink.applyConnectionStatus(
          RobotStatus(
            connectionState: ConnectionStateKind.disconnected,
            robotName: robot.name,
          ),
        );
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
    _telemetryWatchdog?.cancel();
    _telemetryWatchdog = Timer(_watchdogTimeout, () {
      if (_manualDisconnect) return;
      _stateSink.applyConnectionStatus(
        RobotStatus(
          connectionState: ConnectionStateKind.disconnected,
          robotName: robot.name,
          lastError: 'Keine Daten seit ${_watchdogTimeout.inSeconds}s',
        ),
      );
      _scheduleReconnect();
    });

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
      final chargerConnected = decoded['charger_connected'] as bool?;
      final mcuConnected = decoded['mcu_connected'] as bool?;
      final mcuVersion = decoded['mcu_v'] as String?;
      final runtimeHealth = decoded['runtime_health'] as String?;
      final uiMessage = decoded['ui_message'] as String?;
      final uiSeverity = decoded['ui_severity'] as String?;
      final uptimeSeconds = decoded['uptime_s'] as num?;
      final mowFaultPin = decoded['mow_fault_pin'] as bool?;
      final mowDistanceM = decoded['mow_dist_m'] as num?;
      final mowDurationSec = decoded['mow_time_s'] as num?;

      _stateSink.applyTelemetry(
        RobotStatus(
          connectionState: ConnectionStateKind.connected,
          robotName: baseStatus.robotName ?? robot.name,
          batteryPercent: _batteryPercentFromVoltage(
            batteryVoltage?.toDouble(),
          ),
          rtkState: _gpsStateFromTelemetry(gpsSol?.toInt(), gpsText),
          statePhase: statePhase,
          x: x?.toDouble(),
          y: y?.toDouble(),
          heading: heading?.toDouble(),
          gpsLat: gpsLat?.toDouble(),
          gpsLon: gpsLon?.toDouble(),
          piVersion: piVersion ?? baseStatus.piVersion,
          batteryVoltage: batteryVoltage?.toDouble(),
          chargerConnected: chargerConnected,
          mcuConnected: mcuConnected,
          mcuVersion: mcuVersion,
          runtimeHealth: runtimeHealth,
          uptimeSeconds: uptimeSeconds?.toInt(),
          mowFaultActive: mowFaultPin,
          uiMessage: uiMessage,
          uiSeverity: uiSeverity,
          mowDistanceM: mowDistanceM?.toDouble(),
          mowDurationSec: mowDurationSec?.toInt(),
        ),
      );
      _reconnectAttempt = 0;
    } catch (_) {
      // Ignore malformed packets to keep the session stable.
    }
  }

  Future<ConnectionStateKind> _reconnect(SavedRobot robot) async {
    final resolvedConnection = await _probeRobotWithFallback(robot).timeout(
      _reconnectProbeTimeout,
      onTimeout: () => _ResolvedConnection(
        robot: robot,
        status: RobotStatus(
          connectionState: ConnectionStateKind.error,
          robotName: robot.name,
          lastError:
              'Reconnect-Timeout nach ${_reconnectProbeTimeout.inSeconds}s',
        ),
      ),
    );

    final resolvedRobot = resolvedConnection.robot;
    final status = resolvedConnection.status;
    _connectedRobot = resolvedRobot;

    if (status.connectionState != ConnectionStateKind.connected) {
      _stateSink.applyConnectionStatus(status);
      return status.connectionState;
    }

    _reconnectAttempt = 0;
    await _bindTelemetry(resolvedRobot, status);
    _stateSink.applyConnectionStatus(status);
    return ConnectionStateKind.connected;
  }

  void _scheduleReconnect() {
    if (_manualDisconnect) return;
    final robot = _connectedRobot;
    if (robot == null) return;

    _reconnectTimer?.cancel();
    final seconds = 1 << _reconnectAttempt.clamp(0, 4);
    final delay = Duration(seconds: seconds);
    _reconnectAttempt += 1;

    _stateSink.applyConnectionStatus(
      RobotStatus(
        connectionState: ConnectionStateKind.connecting,
        robotName: robot.name,
        lastError: 'Verbindung verloren, neuer Versuch in ${delay.inSeconds}s',
      ),
    );

    _reconnectTimer = Timer(delay, () async {
      if (_manualDisconnect) return;
      final state = await _reconnect(robot);
      if (!_manualDisconnect && state != ConnectionStateKind.connected) {
        _scheduleReconnect();
      }
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
  const _ResolvedConnection({required this.robot, required this.status});

  final SavedRobot robot;
  final RobotStatus status;
}
