import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/mission/mission.dart';
import '../../../domain/robot/saved_robot.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/robot_map_view.dart';

class DashboardScreen extends ConsumerStatefulWidget {
  const DashboardScreen({super.key});

  @override
  ConsumerState<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends ConsumerState<DashboardScreen> {
  bool _commandBusy = false;
  bool _manualModeActive = false;
  String? _lastUiMessage;
  String? _selectedZoneId;
  bool _autoReconnectAttempted = false;

  static const Set<String> _activePhases = <String>{
    'mowing',
    'docking',
    'undocking',
    'navigating_to_start',
    'obstacle_recovery',
    'gps_recovery',
    'waiting_for_rain',
  };

  static const List<DeviceOrientation> _defaultOrientations = <DeviceOrientation>[
    DeviceOrientation.portraitUp,
    DeviceOrientation.portraitDown,
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ];

  static const List<DeviceOrientation> _manualModeOrientations = <DeviceOrientation>[
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ];

  @override
  void dispose() {
    if (_manualModeActive) {
      ref.read(robotConnectionControllerProvider).sendDriveCommand(0, 0);
      unawaited(SystemChrome.setPreferredOrientations(_defaultOrientations));
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final rememberedRobotAsync = ref.watch(rememberedRobotProvider);
    final savedRobotsAsync = ref.watch(savedRobotsProvider);
    final weeklyMowingSummaryAsync = ref.watch(_dashboardWeeklyMowingSummaryProvider);
    final missions = ref.watch(missionsProvider);
    final selectedMissionId = ref.watch(selectedMissionIdProvider);
    final mission = missions.cast<Mission?>().firstWhere(
        (entry) => entry?.id == selectedMissionId,
        orElse: () => missions.isEmpty ? null : missions.first,
      );
    final map = ref.watch(mapGeometryProvider);
    final status = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final savedRobots = savedRobotsAsync.valueOrNull ?? const <SavedRobot>[];
    final isConnected = status.connectionState == ConnectionStateKind.connected;
    final phase = status.statePhase ?? 'idle';
    final isActive = _activePhases.contains(phase);
    final isPaused = phase == 'paused';
    final hasFault = status.connectionState == ConnectionStateKind.error ||
      phase == 'fault' ||
      status.mowFaultActive == true;
    final hasZones = map.zones.isNotEmpty;
    final hasRecurringSchedule = missions.any((entry) => entry.isRecurring);
    final selectedZoneId = mission?.zoneIds.contains(_selectedZoneId) == true
      ? _selectedZoneId
      : null;
    final highlightedZoneIds = selectedZoneId != null
      ? <String>[selectedZoneId]
      : (mission?.zoneIds ?? const <String>[]);

    final hasConnectionNotice = status.lastError != null &&
        (status.connectionState == ConnectionStateKind.error ||
            status.connectionState == ConnectionStateKind.connecting);

    rememberedRobotAsync.whenData((robot) {
      final hasActiveRobot = ref.read(activeRobotProvider) != null;
      final isBusy =
          status.connectionState == ConnectionStateKind.connecting ||
              status.connectionState == ConnectionStateKind.connected;
      if (_autoReconnectAttempted || robot == null || hasActiveRobot || isBusy) {
        return;
      }
      _autoReconnectAttempted = true;
      WidgetsBinding.instance.addPostFrameCallback((_) async {
        final controller = ref.read(robotConnectionControllerProvider);
        await controller.connect(robot, persist: false);
        if (!mounted) return;
        final next = ref.read(connectionStateProvider);
        if (next.connectionState != ConnectionStateKind.connected) {
          controller.stopReconnect();
        }
      });
    });

    ref.listen<RobotStatus>(connectionStateProvider, (previous, next) {
      final msg = next.uiMessage;
      if (msg != null && msg.isNotEmpty && msg != _lastUiMessage) {
        _lastUiMessage = msg;
        final color = switch (next.uiSeverity) {
          'error' => Colors.red.shade700,
          'warn' => Colors.orange.shade700,
          _ => const Color(0xFF1E40AF),
        };
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(msg),
            backgroundColor: color,
            duration: const Duration(seconds: 5),
          ),
        );
      } else if (msg == null || msg.isEmpty) {
        _lastUiMessage = null;
      }
    });

    final hasMapReady = map.perimeter.length >= 3 && map.dock.isNotEmpty;
    final mapAreaSquareMeters = _calculatePolygonAreaSquareMeters(map.perimeter);
    final stateKind = _resolveDashboardStateKind(
      isConnected: isConnected,
      hasRememberedRobot: rememberedRobotAsync.valueOrNull != null,
      hasFault: hasFault,
      isPaused: isPaused,
      hasMapReady: hasMapReady,
      hasZones: hasZones,
      hasRecurringSchedule: hasRecurringSchedule,
    );

    return Scaffold(
      body: Stack(
        children: <Widget>[
          Positioned.fill(
            child: RobotMapView(
              map: map,
              status: status,
              interactive: true,
              selectedZoneIds: highlightedZoneIds,
              onZoneTap: (zoneId) {
                setState(() {
                  _selectedZoneId = _selectedZoneId == zoneId ? null : zoneId;
                });
              },
            ),
          ),
          if (_manualModeActive)
            Positioned.fill(
              child: _ManualDriveMode(
                status: status,
                onClose: _closeManualMode,
                onEmergencyStop: isConnected && !_commandBusy
                    ? () => _runAction(() =>
                        ref.read(robotConnectionControllerProvider).emergencyStop())
                    : null,
                onDriveChanged: (linear, angular) {
                  ref
                      .read(robotConnectionControllerProvider)
                      .sendDriveCommand(linear, angular);
                },
              ),
            )
          else ...<Widget>[
            const Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: <Color>[
                        Color(0xCC09111F),
                        Color(0x0009111F),
                        Color(0x0009111F),
                        Color(0xE609111F),
                      ],
                      stops: <double>[0, 0.24, 0.58, 1],
                    ),
                  ),
                ),
              ),
            ),
            SafeArea(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    _DashboardHeader(
                      status: status,
                      currentRobot: activeRobot ?? rememberedRobotAsync.valueOrNull,
                      savedRobots: savedRobots,
                      mapAreaSquareMeters: mapAreaSquareMeters,
                      onOpenSettings: () => context.push('/settings'),
                      onOpenNotifications: () => context.push('/notifications'),
                      onOpenJoystick: isConnected ? _openManualMode : null,
                      onEmergencyStop: isConnected && !_commandBusy
                          ? () => _runAction(() =>
                              ref.read(robotConnectionControllerProvider).emergencyStop())
                          : null,
                      onRobotSelected: _handleRobotSelected,
                    ),
                    if (hasConnectionNotice) ...<Widget>[
                      const SizedBox(height: 10),
                      ConnectionNotice(status: status),
                    ],
                    const Spacer(),
                    if (isConnected) ...<Widget>[
                      _DashboardTaskLine(
                        stateKind: stateKind,
                        status: status,
                        mission: mission,
                        onTap: mission == null
                            ? () => context.push('/missions')
                            : () => _openMissionOverviewSheet(
                                  context,
                                  missions: missions,
                                  selectedMissionId: selectedMissionId,
                                  mission: mission,
                                  map: map,
                                  selectedZoneId: selectedZoneId,
                                ),
                      ),
                      const SizedBox(height: 10),
                    ],
                    _DashboardControlCard(
                      stateKind: stateKind,
                      status: status,
                      mission: mission,
                      weeklySummary: weeklyMowingSummaryAsync,
                      isConnected: isConnected,
                      isActive: isActive,
                      isPaused: isPaused,
                      isBusy: _commandBusy,
                      onPrimaryAction: _buildPrimaryAction(
                        stateKind: stateKind,
                        mission: mission,
                      ),
                      onDock: () => _runAction(() =>
                          ref.read(robotConnectionControllerProvider).startDocking()),
                      onStop: () => _runAction(() =>
                          ref.read(robotConnectionControllerProvider).emergencyStop()),
                      onStartWholeMap: () => _runAction(
                        () => ref.read(robotConnectionControllerProvider).startMowing(),
                      ),
                      onStatusTap: () => _openDashboardStatusSheet(
                        context,
                        status: status,
                        isConnected: isConnected,
                        isActive: isActive,
                        hasFault: hasFault,
                        isPaused: isPaused,
                        hasPerimeter: map.perimeter.length >= 3,
                        hasDock: map.dock.isNotEmpty,
                        hasZones: hasZones,
                        hasMission: missions.isNotEmpty,
                        hasRecurringSchedule: hasRecurringSchedule,
                        hasRememberedRobot: rememberedRobotAsync.valueOrNull != null,
                        mission: mission,
                      ),
                      onMissionTap: mission == null
                          ? () => context.push('/missions')
                          : () => _openMissionDetails(
                                context,
                                mission,
                                map,
                                selectedZoneId,
                              ),
                      onPlanningTap: () => context.push('/missions'),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Future<void> _openManualMode() async {
    await SystemChrome.setPreferredOrientations(_manualModeOrientations);
    if (!mounted) return;
    setState(() => _manualModeActive = true);
  }

  Future<void> _closeManualMode() async {
    ref.read(robotConnectionControllerProvider).sendDriveCommand(0, 0);
    if (mounted) {
      setState(() => _manualModeActive = false);
    }
    await SystemChrome.setPreferredOrientations(_defaultOrientations);
  }

  VoidCallback? _buildPrimaryAction({
    required _DashboardStateKind stateKind,
    required Mission? mission,
  }) {
    return switch (stateKind) {
      _DashboardStateKind.noRobot => _handleConnectOrSwitch,
      _DashboardStateKind.offline => _handleConnectOrSwitch,
      _DashboardStateKind.fault => () => context.push('/service'),
      _DashboardStateKind.paused => mission == null
          ? () => context.push('/missions')
          : () => _runAction(() {
                ref
                    .read(robotConnectionControllerProvider)
                    .startMowing(missionId: mission.id);
              }),
      _DashboardStateKind.noMap => () => context.push('/map/setup'),
      _DashboardStateKind.noZones => () => context.push('/map/edit'),
        _DashboardStateKind.noSchedule => mission == null
          ? () => context.push('/missions')
          : () => _runAction(() {
            ref
              .read(robotConnectionControllerProvider)
              .startMowing(missionId: mission.id);
            }),
      _DashboardStateKind.ready => mission == null
          ? () => context.push('/missions')
          : () => _runAction(() {
                ref
                    .read(robotConnectionControllerProvider)
                    .startMowing(missionId: mission.id);
              }),
    };
  }

  Future<void> _openDashboardStatusSheet(
    BuildContext context, {
    required RobotStatus status,
    required bool isConnected,
    required bool isActive,
    required bool hasFault,
    required bool isPaused,
    required bool hasPerimeter,
    required bool hasDock,
    required bool hasZones,
    required bool hasMission,
    required bool hasRecurringSchedule,
    required bool hasRememberedRobot,
    required Mission? mission,
  }) async {
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              if (isActive)
                _RunningStateBar(status: status, missionName: mission?.name)
              else
                _ReadyPanel(
                  status: status,
                  isConnected: isConnected,
                  hasFault: hasFault,
                  isPaused: isPaused,
                  hasPerimeter: hasPerimeter,
                  hasDock: hasDock,
                  hasZones: hasZones,
                  hasMission: hasMission,
                  hasRecurringSchedule: hasRecurringSchedule,
                  hasRememberedRobot: hasRememberedRobot,
                  onGoMapSetup: () {
                    Navigator.of(sheetContext).pop();
                    context.push('/map/setup');
                  },
                  onGoMapEdit: () {
                    Navigator.of(sheetContext).pop();
                    context.push('/map/edit');
                  },
                  onGoMissions: () {
                    Navigator.of(sheetContext).pop();
                    context.push('/missions');
                  },
                  onGoService: () {
                    Navigator.of(sheetContext).pop();
                    context.push('/service');
                  },
                  onGoStatistics: () {
                    Navigator.of(sheetContext).pop();
                    context.push('/statistics');
                  },
                  onReconnect: () {
                    Navigator.of(sheetContext).pop();
                    _handleConnectOrSwitch();
                  },
                  onResume: mission == null
                      ? null
                      : () {
                            Navigator.of(sheetContext).pop();
                            _runAction(() {
                              ref
                                  .read(robotConnectionControllerProvider)
                                  .startMowing(missionId: mission.id);
                            });
                          },
                ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _openMissionOverviewSheet(
    BuildContext context, {
    required List<Mission> missions,
    required String? selectedMissionId,
    required Mission mission,
    required MapGeometry map,
    required String? selectedZoneId,
  }) async {
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (sheetContext) => SafeArea(
        child: SingleChildScrollView(
          child: _MissionFocusCard(
            missions: missions,
            mission: mission,
            selectedMissionId: selectedMissionId,
            map: map,
            selectedZoneId: selectedZoneId,
            onMissionSelected: (missionId) {
              ref.read(selectedMissionIdProvider.notifier).state = missionId;
              setState(() => _selectedZoneId = null);
              Navigator.of(sheetContext).pop();
            },
            onZoneSelected: (zoneId) {
              setState(() {
                _selectedZoneId = _selectedZoneId == zoneId ? null : zoneId;
              });
            },
            onOpenDetails: () {
              Navigator.of(sheetContext).pop();
              _openMissionDetails(context, mission, map, selectedZoneId);
            },
            onOpenPlanning: () {
              Navigator.of(sheetContext).pop();
              context.push('/missions');
            },
          ),
        ),
      ),
    );
  }

  Future<void> _openQuickActions(BuildContext context) async {
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(20, 4, 20, 20),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                'Schnellzugriffe',
                style: Theme.of(sheetContext).textTheme.titleLarge,
              ),
              const SizedBox(height: 16),
              _QuickActionTile(
                icon: Icons.map_outlined,
                title: 'Kartensetup',
                subtitle: 'Perimeter, Dock und erste Zonen anlegen',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/map/setup');
                },
              ),
              _QuickActionTile(
                icon: Icons.edit_location_alt_outlined,
                title: 'Karte bearbeiten',
                subtitle: 'Punkte verschieben und Objekte korrigieren',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/map/edit');
                },
              ),
              _QuickActionTile(
                icon: Icons.notifications_outlined,
                title: 'Benachrichtigungen',
                subtitle: 'Warnungen, Hinweise und aktuelle Meldungen ansehen',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/notifications');
                },
              ),
              _QuickActionTile(
                icon: Icons.settings_outlined,
                title: 'Einstellungen',
                subtitle: 'App- und Roboterdetails als sekundären Bereich öffnen',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/settings');
                },
              ),
              _QuickActionTile(
                icon: Icons.route_outlined,
                title: 'Planung',
                subtitle: 'Missionen, Zonen und Vorschau verwalten',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/missions');
                },
              ),
              _QuickActionTile(
                icon: Icons.miscellaneous_services_outlined,
                title: 'Service',
                subtitle: 'Updates, Diagnose und Systemfunktionen öffnen',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/service');
                },
              ),
              _QuickActionTile(
                icon: Icons.insights_outlined,
                title: 'Statistik',
                subtitle: 'Leistung und Historie prüfen',
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  context.push('/statistics');
                },
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _openMissionDetails(
    BuildContext context,
    Mission mission,
    MapGeometry map,
    String? selectedZoneId,
  ) async {
    final zoneNames = map.zones
        .where((zone) => mission.zoneIds.contains(zone.id))
        .map((zone) => zone.name)
        .toList(growable: false);
    final selectedZoneName = selectedZoneId == null
        ? null
        : map.zones
            .where((zone) => zone.id == selectedZoneId)
            .map((zone) => zone.name)
            .cast<String?>()
            .firstWhere((zone) => zone != null, orElse: () => null);

    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(20, 4, 20, 20),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                mission.name,
                style: Theme.of(sheetContext).textTheme.titleLarge,
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: <Widget>[
                  _MissionMetaPill(
                    icon: Icons.crop_square_rounded,
                    label: '${mission.zoneIds.length} Zonen',
                  ),
                  _MissionMetaPill(
                    icon: Icons.schedule_rounded,
                    label: mission.effectiveScheduleLabel,
                  ),
                  _MissionMetaPill(
                    icon: Icons.pattern_rounded,
                    label: mission.pattern,
                  ),
                ],
              ),
              const SizedBox(height: 16),
              Text(
                'Zonen',
                style: Theme.of(sheetContext).textTheme.titleMedium,
              ),
              const SizedBox(height: 8),
              if (zoneNames.isEmpty)
                const Text('Noch keine Zonen in dieser Mission ausgewählt.')
              else
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: zoneNames
                      .map(
                        (name) => _MissionMetaPill(
                          icon: selectedZoneName == name
                              ? Icons.place_rounded
                              : Icons.grass_rounded,
                          label: name,
                          emphasized: selectedZoneName == name,
                        ),
                      )
                      .toList(growable: false),
                ),
              const SizedBox(height: 16),
              Text(
                'Bedingungen',
                style: Theme.of(sheetContext).textTheme.titleMedium,
              ),
              const SizedBox(height: 8),
              Text(mission.onlyWhenDry ? 'Nur trocken aktiviert' : 'Mäht auch bei unklarem Wetter'),
              const SizedBox(height: 4),
              Text(
                mission.requiresHighBattery
                    ? 'Start erst ab hohem Akkustand'
                    : 'Kein hoher Akkustand erforderlich',
              ),
              const SizedBox(height: 16),
              Row(
                children: <Widget>[
                  Expanded(
                    child: FilledButton.tonalIcon(
                      onPressed: () {
                        Navigator.of(sheetContext).pop();
                        context.go('/missions');
                      },
                      icon: const Icon(Icons.route_outlined),
                      label: const Text('Planung öffnen'),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _runAction(VoidCallback action) async {
    if (_commandBusy) return;
    setState(() => _commandBusy = true);
    try {
      action();
    } catch (_) {
      // Prevent _commandBusy from staying true if the action throws synchronously.
    }
    await Future<void>.delayed(const Duration(milliseconds: 350));
    if (mounted) setState(() => _commandBusy = false);
  }

  Future<void> _handleDisconnect() async {
    await ref.read(robotConnectionControllerProvider).disconnect();
    if (!mounted) return;
    context.go('/discover');
  }

  Future<void> _handleConnectOrSwitch() async {
    if (ref.read(activeRobotProvider) == null) {
      if (!mounted) return;
      context.push('/discover');
      return;
    }
    await _handleSwitchRobot();
  }

  Future<void> _handleSwitchRobot() async {
    await ref.read(robotConnectionControllerProvider).switchRobot();
    if (!mounted) return;
    context.go('/discover');
  }

  Future<void> _handleRobotSelected(SavedRobot robot) async {
    final currentRobot = ref.read(activeRobotProvider) ?? ref.read(rememberedRobotProvider).valueOrNull;
    if (currentRobot?.id == robot.id) {
      return;
    }

    final refreshedRobot = SavedRobot(
      id: robot.id,
      name: robot.name,
      lastHost: robot.lastHost,
      port: robot.port,
      lastSeen: DateTime.now(),
    );

    await ref.read(robotConnectionControllerProvider).connect(refreshedRobot);
  }
}

enum _MenuAction { switchRobot, disconnect }

double _calculatePolygonAreaSquareMeters(List<MapPoint> points) {
  if (points.length < 3) {
    return 0;
  }

  var doubledArea = 0.0;
  for (var index = 0; index < points.length; index++) {
    final current = points[index];
    final next = points[(index + 1) % points.length];
    doubledArea += (current.x * next.y) - (next.x * current.y);
  }
  return doubledArea.abs() / 2;
}

String _formatAreaSquareMeters(double area) {
  if (area <= 0) {
    return '-- m²';
  }
  return '${area.round()} m²';
}

String _rtkHeaderLabel(String? rtkState) {
  return switch (rtkState) {
    'RTK Fixed' => 'Fix',
    'RTK Float' => 'Float',
    null || '' => 'Suche',
    _ => rtkState,
  };
}

_DashboardStateKind _resolveDashboardStateKind({
  required bool isConnected,
  required bool hasRememberedRobot,
  required bool hasFault,
  required bool isPaused,
  required bool hasMapReady,
  required bool hasZones,
  required bool hasRecurringSchedule,
}) {
  if (!isConnected) {
    return hasRememberedRobot
        ? _DashboardStateKind.offline
        : _DashboardStateKind.noRobot;
  }
  if (hasFault) return _DashboardStateKind.fault;
  if (isPaused) return _DashboardStateKind.paused;
  if (!hasMapReady) return _DashboardStateKind.noMap;
  if (!hasZones) return _DashboardStateKind.noZones;
  if (!hasRecurringSchedule) return _DashboardStateKind.noSchedule;
  return _DashboardStateKind.ready;
}

class _DashboardHeader extends StatelessWidget {
  const _DashboardHeader({
    required this.status,
    required this.currentRobot,
    required this.savedRobots,
    required this.mapAreaSquareMeters,
    required this.onOpenSettings,
    required this.onOpenNotifications,
    required this.onEmergencyStop,
    required this.onOpenJoystick,
    required this.onRobotSelected,
  });

  final RobotStatus status;
  final SavedRobot? currentRobot;
  final List<SavedRobot> savedRobots;
  final double mapAreaSquareMeters;
  final VoidCallback onOpenSettings;
  final VoidCallback onOpenNotifications;
  final VoidCallback? onEmergencyStop;
  final VoidCallback? onOpenJoystick;
  final ValueChanged<SavedRobot> onRobotSelected;

  @override
  Widget build(BuildContext context) {
    final battery = status.batteryPercent != null ? '${status.batteryPercent}%' : '--';
    final connectionLabel = switch (status.connectionState) {
      ConnectionStateKind.connected => 'Verbunden',
      ConnectionStateKind.connecting => 'Verbinde',
      ConnectionStateKind.error => 'Fehler',
      _ => 'Offline',
    };
    final connectionColor = switch (status.connectionState) {
      ConnectionStateKind.connected => const Color(0xFF4ADE80),
      ConnectionStateKind.connecting => const Color(0xFFFACC15),
      ConnectionStateKind.error => const Color(0xFFF87171),
      _ => const Color(0xFF94A3B8),
    };
    final rtkLabel = _rtkHeaderLabel(status.rtkState);
    final (gpsPillLabel, gpsColor) = _StatusOverlay._gpsInfo(status.rtkState);

    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Row(
                children: <Widget>[
                  _RoundOverlayButton(
                    tooltip: 'Einstellungen',
                    icon: Icons.settings_outlined,
                    onPressed: onOpenSettings,
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: _RobotSelectorChip(
                      currentRobot: currentRobot,
                      savedRobots: savedRobots,
                      onRobotSelected: onRobotSelected,
                    ),
                  ),
                  const SizedBox(width: 10),
                  _RoundOverlayButton(
                    tooltip: 'Benachrichtigungen',
                    icon: Icons.notifications_none_rounded,
                    onPressed: onOpenNotifications,
                  ),
                  const SizedBox(width: 8),
                  _RoundOverlayButton(
                    tooltip: 'Not-Aus',
                    icon: Icons.stop_circle_outlined,
                    onPressed: onEmergencyStop,
                    backgroundColor: const Color(0xD9DC2626),
                    iconColor: Colors.white,
                  ),
                ],
              ),
              const SizedBox(height: 10),
              SingleChildScrollView(
                scrollDirection: Axis.horizontal,
                child: Row(
                  children: <Widget>[
                    _HeaderStatusPill(
                      label: 'Verbindung',
                      value: connectionLabel,
                      color: connectionColor,
                    ),
                    const SizedBox(width: 8),
                    _HeaderStatusPill(
                      label: 'Akku',
                      value: battery,
                      color: _StatusOverlay._batteryColor(status.batteryPercent),
                    ),
                    const SizedBox(width: 8),
                    _HeaderStatusPill(
                      label: 'RTK',
                      value: rtkLabel,
                      color: gpsColor,
                      icon: Icons.gps_fixed_rounded,
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
        const SizedBox(width: 12),
        _HeaderSideRail(
          mapAreaSquareMeters: mapAreaSquareMeters,
          onOpenJoystick: onOpenJoystick,
        ),
      ],
    );
  }
}

class _RobotSelectorChip extends StatelessWidget {
  const _RobotSelectorChip({
    required this.currentRobot,
    required this.savedRobots,
    required this.onRobotSelected,
  });

  final SavedRobot? currentRobot;
  final List<SavedRobot> savedRobots;
  final ValueChanged<SavedRobot> onRobotSelected;

  @override
  Widget build(BuildContext context) {
    final title = currentRobot?.name ?? 'Roboter auswählen';
    final hasChoices = savedRobots.length > 1;

    final child = Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
      decoration: BoxDecoration(
        color: const Color(0xC20F172A),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: const Color(0x33FFFFFF)),
      ),
      child: Row(
        children: <Widget>[
          Expanded(
            child: Text(
              title,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w700,
                    color: Colors.white,
                  ),
            ),
          ),
          if (hasChoices) ...<Widget>[
            const SizedBox(width: 8),
            const Icon(
              Icons.keyboard_arrow_down_rounded,
              color: Color(0xFFCBD5E1),
            ),
          ],
        ],
      ),
    );

    if (!hasChoices) {
      return child;
    }

    return PopupMenuButton<String>(
      tooltip: 'Roboter auswählen',
      color: const Color(0xEE0F172A),
      onSelected: (robotId) {
        for (final robot in savedRobots) {
          if (robot.id == robotId) {
            onRobotSelected(robot);
            break;
          }
        }
      },
      itemBuilder: (context) => savedRobots
          .map(
            (robot) => PopupMenuItem<String>(
              value: robot.id,
              child: Row(
                children: <Widget>[
                  Expanded(child: Text(robot.name)),
                  if (robot.id == currentRobot?.id)
                    const Icon(Icons.check_rounded, size: 16),
                ],
              ),
            ),
          )
          .toList(growable: false),
      child: child,
    );
  }
}

class _HeaderStatusPill extends StatelessWidget {
  const _HeaderStatusPill({
    required this.label,
    required this.value,
    required this.color,
    this.icon,
  });

  final String label;
  final String value;
  final Color color;
  final IconData? icon;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: const Color(0xB80F172A),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: const Color(0x33FFFFFF)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          if (icon != null) ...<Widget>[
            Icon(icon, size: 14, color: color),
            const SizedBox(width: 6),
          ],
          Text(
            '$label ',
            style: const TextStyle(
              color: Color(0xFFCBD5E1),
              fontSize: 12,
              fontWeight: FontWeight.w600,
            ),
          ),
          Text(
            value,
            style: TextStyle(
              color: color,
              fontWeight: FontWeight.w700,
              fontSize: 12,
            ),
          ),
        ],
      ),
    );
  }
}

class _HeaderSideRail extends StatelessWidget {
  const _HeaderSideRail({
    required this.mapAreaSquareMeters,
    required this.onOpenJoystick,
  });

  final double mapAreaSquareMeters;
  final VoidCallback? onOpenJoystick;

  @override
  Widget build(BuildContext context) {
    return Column(
      children: <Widget>[
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
          decoration: BoxDecoration(
            color: const Color(0xC20F172A),
            borderRadius: BorderRadius.circular(18),
            border: Border.all(color: const Color(0x33FFFFFF)),
          ),
          child: Column(
            children: <Widget>[
              const Icon(Icons.crop_square_rounded, color: Colors.white, size: 18),
              const SizedBox(height: 4),
              Text(
                _formatAreaSquareMeters(mapAreaSquareMeters),
                style: Theme.of(context).textTheme.labelSmall?.copyWith(
                      color: Colors.white,
                      fontWeight: FontWeight.w700,
                    ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 10),
        _RoundOverlayButton(
          tooltip: 'Joystick',
          icon: Icons.gamepad_rounded,
          onPressed: onOpenJoystick,
        ),
      ],
    );
  }
}

class _RoundOverlayButton extends StatelessWidget {
  const _RoundOverlayButton({
    required this.tooltip,
    required this.icon,
    this.onPressed,
    this.backgroundColor = const Color(0xC20F172A),
    this.iconColor = Colors.white,
  });

  final String tooltip;
  final IconData icon;
  final VoidCallback? onPressed;
  final Color backgroundColor;
  final Color iconColor;

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tooltip,
      child: Material(
        color: backgroundColor,
        shape: const CircleBorder(),
        child: IconButton(
          onPressed: onPressed,
          icon: Icon(icon, color: iconColor),
        ),
      ),
    );
  }
}

class _CompactMissionStrip extends StatelessWidget {
  const _CompactMissionStrip({
    required this.mission,
    required this.map,
    required this.selectedZoneId,
    required this.onTap,
  });

  final Mission mission;
  final MapGeometry map;
  final String? selectedZoneId;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final zoneNames = map.zones
        .where((zone) => mission.zoneIds.contains(zone.id))
        .map((zone) => zone.id == selectedZoneId ? '${zone.name} aktiv' : zone.name)
        .toList(growable: false);

    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(22),
        child: Container(
          width: double.infinity,
          padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
          decoration: BoxDecoration(
            color: const Color(0xD90F172A),
            borderRadius: BorderRadius.circular(20),
            border: Border.all(color: const Color(0x331E40AF)),
          ),
          child: Row(
            children: <Widget>[
              Container(
                width: 38,
                height: 38,
                decoration: BoxDecoration(
                  color: const Color(0x1F60A5FA),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: const Icon(Icons.route_rounded, color: Color(0xFF93C5FD), size: 18),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    Text(
                      mission.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: Theme.of(context).textTheme.bodyLarge?.copyWith(
                            color: Colors.white,
                            fontWeight: FontWeight.w700,
                          ),
                    ),
                    const SizedBox(height: 1),
                    Text(
                      zoneNames.isEmpty
                          ? mission.effectiveScheduleLabel
                          : zoneNames.take(2).join(' • '),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                            color: const Color(0xFFCBD5E1),
                          ),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 8),
              const Icon(
                Icons.keyboard_arrow_up_rounded,
                color: Color(0xFF94A3B8),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

final _dashboardWeeklyMowingSummaryProvider =
    FutureProvider<_WeeklyMowingSummary>((ref) async {
  final robot = ref.watch(activeRobotProvider);
  if (robot == null) {
    return const _WeeklyMowingSummary(totalSeconds: 0, sessionCount: 0);
  }

  try {
    final raw = await ref.watch(robotApiProvider).getHistoryEvents(
          host: robot.lastHost,
          port: robot.port,
          limit: 200,
        );
    final events = raw.whereType<Map<String, dynamic>>().toList(growable: false);
    final sessions = _parseMowSessions(events);
    final now = DateTime.now();
    final startOfWeek = DateTime(now.year, now.month, now.day)
        .subtract(Duration(days: now.weekday - 1));
    final startOfWeekMs = startOfWeek.millisecondsSinceEpoch;

    var totalSeconds = 0;
    var sessionCount = 0;
    for (final session in sessions) {
      if (session.endMs <= startOfWeekMs) {
        continue;
      }
      final boundedStartMs = session.startMs < startOfWeekMs
          ? startOfWeekMs
          : session.startMs;
      final durationMs = session.endMs - boundedStartMs;
      if (durationMs <= 0) {
        continue;
      }
      totalSeconds += durationMs ~/ 1000;
      sessionCount += 1;
    }

    return _WeeklyMowingSummary(
      totalSeconds: totalSeconds,
      sessionCount: sessionCount,
    );
  } catch (_) {
    return const _WeeklyMowingSummary(totalSeconds: 0, sessionCount: 0);
  }
});

List<_MowSessionWindow> _parseMowSessions(List<Map<String, dynamic>> events) {
  final sorted = List<Map<String, dynamic>>.from(events)
    ..sort((a, b) {
      final ta = a['timestamp'] as num? ?? a['time'] as num? ?? 0;
      final tb = b['timestamp'] as num? ?? b['time'] as num? ?? 0;
      return ta.compareTo(tb);
    });

  final result = <_MowSessionWindow>[];
  int? sessionStart;

  for (final event in sorted) {
    final phase = event['phase'] as String?;
    final timestamp = (event['timestamp'] as num? ?? event['time'] as num?)?.toInt();
    if (timestamp == null) {
      continue;
    }

    if (phase == 'mowing' && sessionStart == null) {
      sessionStart = timestamp;
      continue;
    }

    if (phase != 'mowing' && sessionStart != null) {
      result.add(_MowSessionWindow(startMs: sessionStart, endMs: timestamp));
      sessionStart = null;
    }
  }

  if (sessionStart != null && sorted.isNotEmpty) {
    final lastTimestamp =
        (sorted.last['timestamp'] as num? ?? sorted.last['time'] as num?)?.toInt();
    if (lastTimestamp != null && lastTimestamp > sessionStart) {
      result.add(_MowSessionWindow(startMs: sessionStart, endMs: lastTimestamp));
    }
  }

  return result;
}

class _WeeklyMowingSummary {
  const _WeeklyMowingSummary({
    required this.totalSeconds,
    required this.sessionCount,
  });

  final int totalSeconds;
  final int sessionCount;
}

class _MowSessionWindow {
  const _MowSessionWindow({
    required this.startMs,
    required this.endMs,
  });

  final int startMs;
  final int endMs;
}

class _DashboardTaskLine extends StatelessWidget {
  const _DashboardTaskLine({
    required this.stateKind,
    required this.status,
    required this.mission,
    required this.onTap,
  });

  final _DashboardStateKind stateKind;
  final RobotStatus status;
  final Mission? mission;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final label = switch (stateKind) {
      _DashboardStateKind.noMap => 'Nächster Schritt: Karte anlegen',
      _DashboardStateKind.noZones => 'Nächster Schritt: Zonen anlegen',
      _DashboardStateKind.noSchedule => 'Nächste Aufgabe: Zeitplan festlegen',
      _DashboardStateKind.paused => 'Aktuelle Aufgabe: Lauf pausiert',
      _DashboardStateKind.ready => mission == null
          ? 'Aktuelle Aufgabe: Mission wählen'
          : 'Aktuelle Aufgabe: ${mission!.name}',
      _DashboardStateKind.fault => 'Aktuelle Aufgabe: Fehler prüfen',
      _DashboardStateKind.offline => 'Aktuelle Aufgabe: Verbindung wiederherstellen',
      _DashboardStateKind.noRobot => 'Aktuelle Aufgabe: Roboter verbinden',
    };
    final detail = status.statePhase == null || status.statePhase == 'idle'
        ? null
        : _StatusOverlay._phaseLabel(status.statePhase!);

    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(16),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 2),
          child: Row(
            children: <Widget>[
              const Icon(
                Icons.route_rounded,
                size: 16,
                color: Color(0xFFE2E8F0),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  detail == null ? label : '$label • $detail',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                        color: Colors.white,
                        fontWeight: FontWeight.w600,
                      ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _DashboardControlCard extends StatelessWidget {
  const _DashboardControlCard({
    required this.stateKind,
    required this.status,
    required this.mission,
    required this.weeklySummary,
    required this.isConnected,
    required this.isActive,
    required this.isPaused,
    required this.isBusy,
    required this.onPrimaryAction,
    required this.onDock,
    required this.onStop,
    required this.onStartWholeMap,
    required this.onStatusTap,
    required this.onMissionTap,
    required this.onPlanningTap,
  });

  final _DashboardStateKind stateKind;
  final RobotStatus status;
  final Mission? mission;
  final AsyncValue<_WeeklyMowingSummary> weeklySummary;
  final bool isConnected;
  final bool isActive;
  final bool isPaused;
  final bool isBusy;
  final VoidCallback? onPrimaryAction;
  final VoidCallback onDock;
  final VoidCallback onStop;
  final VoidCallback onStartWholeMap;
  final VoidCallback onStatusTap;
  final VoidCallback onMissionTap;
  final VoidCallback onPlanningTap;

  @override
  Widget build(BuildContext context) {
    final progressTitle = _buildProgressTitle();
    final progressValue = _buildProgressValue();
    final primaryLabel = _buildPrimaryLabel();
    final primaryEnabled = _isPrimaryEnabled();

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.fromLTRB(18, 16, 18, 18),
      decoration: BoxDecoration(
        color: const Color(0xFFF8FAFC),
        borderRadius: BorderRadius.circular(26),
        border: Border.all(color: const Color(0xFFE2E8F0)),
        boxShadow: const <BoxShadow>[
          BoxShadow(
            color: Color(0x260F172A),
            blurRadius: 20,
            offset: Offset(0, 10),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          if (stateKind == _DashboardStateKind.noMap)
            Container(
              width: double.infinity,
              padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
              decoration: BoxDecoration(
                color: const Color(0xFFF0FDF4),
                borderRadius: BorderRadius.circular(20),
                border: Border.all(color: const Color(0xFF86EFAC)),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    'Es ist noch keine Karte angelegt.',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                          color: const Color(0xFF0F172A),
                          fontWeight: FontWeight.w700,
                        ),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    'Bitte zuerst Perimeter, No-Go-Bereiche und Docking festlegen.',
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                          color: const Color(0xFF334155),
                          fontWeight: FontWeight.w500,
                        ),
                  ),
                  const SizedBox(height: 10),
                  const Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: <Widget>[
                      _StepBadge(label: 'Perimeter'),
                      _StepBadge(label: 'No-Go'),
                      _StepBadge(label: 'Docking'),
                    ],
                  ),
                ],
              ),
            )
          else
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    Expanded(
                      child: _PanelMetricCard(
                        title: 'Diese Woche gemäht',
                        value: weeklySummary.when(
                          data: (summary) => summary.totalSeconds == 0
                              ? '0 h'
                              : _RunningStateBar._formatDuration(summary.totalSeconds),
                          loading: () => 'Lädt …',
                          error: (_, __) => '0 h',
                        ),
                        subtitle: weeklySummary.when(
                          data: (summary) => summary.sessionCount == 1
                              ? '1 Einsatz'
                              : '${summary.sessionCount} Einsätze',
                          loading: () => 'Verlauf wird geladen',
                          error: (_, __) => 'Verlauf nicht verfügbar',
                        ),
                        onTap: onStatusTap,
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: _PanelMetricCard(
                        title: progressTitle,
                        value: progressValue,
                        subtitle: _buildProgressSubtitle(),
                        onTap: mission == null ? onPlanningTap : onMissionTap,
                        trailing: _buildProgressTrailing(context),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 14),
                if (_shouldShowSchedulerLink())
                  Align(
                    alignment: Alignment.centerLeft,
                    child: TextButton.icon(
                      onPressed: onPlanningTap,
                      icon: const Icon(Icons.schedule_rounded, size: 18),
                      label: const Text('Zeitplan anlegen'),
                      style: TextButton.styleFrom(
                        foregroundColor: const Color(0xFF16A34A),
                        padding: EdgeInsets.zero,
                      ),
                    ),
                  )
                else if (_shouldShowWholeMapAction())
                  Align(
                    alignment: Alignment.centerLeft,
                    child: OutlinedButton.icon(
                      onPressed: onStartWholeMap,
                      icon: const Icon(Icons.landscape_outlined, size: 18),
                      label: const Text('Gesamte Karte mähen'),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: const Color(0xFF0F172A),
                        side: const BorderSide(color: Color(0xFFCBD5E1)),
                        padding: const EdgeInsets.symmetric(
                          horizontal: 14,
                          vertical: 10,
                        ),
                      ),
                    ),
                  ),
              ],
            ),
          const SizedBox(height: 12),
          if (stateKind == _DashboardStateKind.noMap)
            SizedBox(
              width: double.infinity,
              child: FilledButton.icon(
                onPressed: isConnected && !isBusy ? onPrimaryAction : null,
                icon: const Icon(Icons.map_outlined),
                style: FilledButton.styleFrom(
                  backgroundColor: const Color(0xFF22C55E),
                  foregroundColor: const Color(0xFF04110A),
                  padding: const EdgeInsets.symmetric(vertical: 16),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(18),
                  ),
                ),
                label: Text(
                  primaryLabel,
                  style: const TextStyle(
                    fontSize: 17,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 0.2,
                  ),
                ),
              ),
            )
          else
            Row(
              children: <Widget>[
                Expanded(
                  child: FilledButton(
                    onPressed: primaryEnabled ? onPrimaryAction : null,
                    style: FilledButton.styleFrom(
                      backgroundColor: _isStopAction()
                          ? const Color(0xFFDC2626)
                          : const Color(0xFF16A34A),
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 15),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(18),
                      ),
                    ),
                    child: Text(
                      primaryLabel,
                      style: const TextStyle(
                        fontSize: 17,
                        fontWeight: FontWeight.w800,
                        letterSpacing: 0.2,
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: OutlinedButton(
                    onPressed: isConnected && !isBusy ? onDock : null,
                    style: OutlinedButton.styleFrom(
                      foregroundColor: const Color(0xFF0F172A),
                      side: const BorderSide(color: Color(0xFFCBD5E1)),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(18),
                      ),
                    ),
                    child: const Text('Dock / Heim'),
                  ),
                ),
              ],
            ),
        ],
      ),
    );
  }

  String _buildProgressTitle() {
        if (isActive || isPaused) {
          return 'Aktueller Lauf';
        }
        if (mission?.scheduleHour != null) {
          return 'Nächster Start';
        }
        if (mission?.isRecurring == true) {
          return 'Zeitplan';
        }
        return 'Startzeit';
  }

  String _buildProgressValue() {
        if (isActive || isPaused) {
          if (status.mowDurationSec != null) {
            return _RunningStateBar._formatDuration(status.mowDurationSec!);
          }
          return _StatusOverlay._phaseLabel(status.statePhase ?? 'idle');
        }
        if (mission?.scheduleHour != null) {
          final hour = mission!.scheduleHour!.toString().padLeft(2, '0');
          final minute = (mission!.scheduleMinute ?? 0).toString().padLeft(2, '0');
          return '$hour:$minute';
        }
        if (mission?.isRecurring == true) {
          return mission!.effectiveScheduleLabel;
        }
        return 'Kein Zeitplan';
  }

  String _buildProgressSubtitle() {
        if (isActive || isPaused) {
          if (status.mowDistanceM != null) {
            return '${status.mowDistanceM!.toStringAsFixed(0)} m gemäht';
          }
          return mission?.name ?? 'Mähvorgang läuft';
        }
        if (mission?.scheduleHour != null) {
          return mission?.effectiveScheduleLabel ?? 'Wiederkehrender Start';
        }
        if (mission == null) {
          return 'Bitte zuerst eine Mission wählen';
        }
        return 'Direktlink zum Zeitplan';
  }

  Widget? _buildProgressTrailing(BuildContext context) {
        if (!_shouldShowSchedulerLink()) {
          return null;
        }
        return Text(
          'Zeitplan',
          style: Theme.of(context).textTheme.labelMedium?.copyWith(
                color: const Color(0xFF16A34A),
                fontWeight: FontWeight.w700,
              ),
        );
  }

  bool _shouldShowSchedulerLink() {
    return !isActive && !isPaused && (mission == null || mission?.scheduleHour == null);
  }

  bool _shouldShowWholeMapAction() {
    return !isActive &&
        !isPaused &&
        isConnected &&
        !isBusy &&
        stateKind != _DashboardStateKind.noRobot &&
        stateKind != _DashboardStateKind.offline &&
        stateKind != _DashboardStateKind.fault &&
        stateKind != _DashboardStateKind.noMap;
  }

  bool _isStopAction() {
    return isActive && stateKind != _DashboardStateKind.paused;
  }

  String _buildPrimaryLabel() {
    if (_isStopAction()) {
      return 'Stop';
    }
    return switch (stateKind) {
      _DashboardStateKind.noRobot => 'Verbinden',
      _DashboardStateKind.offline => 'Neu verbinden',
      _DashboardStateKind.fault => 'Service',
      _DashboardStateKind.paused => mission == null ? 'Mission öffnen' : 'Fortsetzen',
      _DashboardStateKind.noMap => 'Karte anlegen',
      _DashboardStateKind.noZones => 'Zonen anlegen',
      _DashboardStateKind.noSchedule => mission == null ? 'Mission anlegen' : 'Start',
      _DashboardStateKind.ready => mission == null ? 'Mission öffnen' : 'Start',
    };
  }

  bool _isPrimaryEnabled() {
    if (_isStopAction()) {
      return isConnected && !isBusy;
    }
    return isConnected && !isBusy ||
        stateKind == _DashboardStateKind.noRobot ||
        stateKind == _DashboardStateKind.offline ||
        stateKind == _DashboardStateKind.fault ||
        stateKind == _DashboardStateKind.noMap;
  }
}


    class _PanelMetricCard extends StatelessWidget {
      const _PanelMetricCard({
        required this.title,
        required this.value,
        required this.subtitle,
        required this.onTap,
        this.trailing,
      });

      final String title;
      final String value;
      final String subtitle;
      final VoidCallback onTap;
      final Widget? trailing;

      @override
      Widget build(BuildContext context) {
        return Material(
          color: const Color(0xFFF8FAFC),
          child: InkWell(
            onTap: onTap,
            borderRadius: BorderRadius.circular(18),
            child: Container(
              padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
              decoration: BoxDecoration(
                color: const Color(0xFFF8FAFC),
                borderRadius: BorderRadius.circular(18),
                border: Border.all(color: const Color(0xFFE2E8F0)),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    title,
                    style: Theme.of(context).textTheme.labelMedium?.copyWith(
                          color: const Color(0xFF64748B),
                          fontWeight: FontWeight.w700,
                        ),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    value,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.titleLarge?.copyWith(
                          color: const Color(0xFF0F172A),
                          fontWeight: FontWeight.w800,
                        ),
                  ),
                  const SizedBox(height: 4),
                  Row(
                    children: <Widget>[
                      Expanded(
                        child: Text(
                          subtitle,
                          maxLines: 2,
                          overflow: TextOverflow.ellipsis,
                          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                                color: const Color(0xFF475569),
                              ),
                        ),
                      ),
                      if (trailing != null) ...<Widget>[
                        const SizedBox(width: 8),
                        trailing!,
                      ],
                    ],
                  ),
                ],
              ),
            ),
          ),
        );
      }
    }
class _StepBadge extends StatelessWidget {
  const _StepBadge({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: const Color(0x1F0F172A),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: const Color(0x33475569)),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.labelMedium?.copyWith(
              color: const Color(0xFFE2E8F0),
              fontWeight: FontWeight.w700,
            ),
      ),
    );
  }
}

class _MiniInfoCard extends StatelessWidget {
  const _MiniInfoCard({
    required this.title,
    required this.value,
    required this.onTap,
  });

  final String title;
  final String value;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xFF111A2C),
      borderRadius: BorderRadius.circular(18),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(18),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                title,
                style: Theme.of(context).textTheme.labelMedium?.copyWith(
                      color: const Color(0xFF93C5FD),
                    ),
              ),
              const SizedBox(height: 4),
              Text(
                value,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      color: Colors.white,
                      fontWeight: FontWeight.w700,
                    ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _InfoBadge extends StatelessWidget {
  const _InfoBadge({
    required this.label,
    required this.icon,
  });

  final String label;
  final IconData icon;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: const Color(0xFF111A2C),
        borderRadius: BorderRadius.circular(16),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Icon(icon, size: 16, color: const Color(0xFF4ADE80)),
          const SizedBox(width: 6),
          Text(
            label,
            style: const TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
    );
  }
}

class _MissionFocusCard extends StatelessWidget {
  const _MissionFocusCard({
    required this.missions,
    required this.mission,
    required this.selectedMissionId,
    required this.map,
    required this.selectedZoneId,
    required this.onMissionSelected,
    required this.onZoneSelected,
    required this.onOpenDetails,
    required this.onOpenPlanning,
  });

  final List<Mission> missions;
  final Mission mission;
  final String? selectedMissionId;
  final MapGeometry map;
  final String? selectedZoneId;
  final void Function(String missionId) onMissionSelected;
  final void Function(String zoneId) onZoneSelected;
  final VoidCallback onOpenDetails;
  final VoidCallback onOpenPlanning;

  @override
  Widget build(BuildContext context) {
    final missionZones = map.zones
        .where((zone) => mission.zoneIds.contains(zone.id))
        .toList(growable: false);

    return Container(
      width: double.infinity,
      color: const Color(0xFF0A1020),
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              Expanded(
                child: Text(
                  'Einsatz',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.w700,
                      ),
                ),
              ),
              TextButton(
                onPressed: onOpenDetails,
                child: const Text('Details'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: Row(
              children: missions
                  .map(
                    (entry) => Padding(
                      padding: const EdgeInsets.only(right: 8),
                      child: ChoiceChip(
                        label: Text(entry.name),
                        selected: entry.id == (selectedMissionId ?? mission.id),
                        onSelected: (_) => onMissionSelected(entry.id),
                      ),
                    ),
                  )
                  .toList(growable: false),
            ),
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: <Widget>[
              _MissionMetaPill(
                icon: Icons.schedule_rounded,
                label: mission.effectiveScheduleLabel,
              ),
              _MissionMetaPill(
                icon: Icons.pattern_rounded,
                label: mission.pattern,
              ),
              _MissionMetaPill(
                icon: Icons.crop_square_rounded,
                label: '${mission.zoneIds.length} Zonen',
              ),
            ],
          ),
          const SizedBox(height: 12),
          Text(
            'Zonen',
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  fontWeight: FontWeight.w600,
                ),
          ),
          const SizedBox(height: 8),
          if (missionZones.isEmpty)
            Text(
              'Noch keine Zonen gewählt. Öffne die Planung und stelle eine Mission zusammen.',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: const Color(0xFF94A3B8),
                  ),
            )
          else
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: missionZones
                  .map(
                    (zone) => FilterChip(
                      label: Text(zone.name),
                      selected: zone.id == selectedZoneId,
                      onSelected: (_) => onZoneSelected(zone.id),
                    ),
                  )
                  .toList(growable: false),
            ),
          const SizedBox(height: 12),
          Align(
            alignment: Alignment.centerRight,
            child: FilledButton.tonalIcon(
              onPressed: onOpenPlanning,
              icon: const Icon(Icons.route_outlined),
              label: const Text('Planung öffnen'),
            ),
          ),
        ],
      ),
    );
  }
}

class _MissionMetaPill extends StatelessWidget {
  const _MissionMetaPill({
    required this.icon,
    required this.label,
    this.emphasized = false,
  });

  final IconData icon;
  final String label;
  final bool emphasized;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: emphasized ? const Color(0x1F2563EB) : const Color(0xFF0F1829),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(
          color: emphasized ? const Color(0xFF60A5FA) : const Color(0xFF1E3A5F),
        ),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Icon(icon, size: 14, color: const Color(0xFF60A5FA)),
          const SizedBox(width: 6),
          Text(label),
        ],
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Status overlay chips (map top-left)
// ---------------------------------------------------------------------------

class _StatusOverlay extends StatelessWidget {
  const _StatusOverlay({required this.status});
  final RobotStatus status;

  @override
  Widget build(BuildContext context) {
    final (gpsLabel, gpsColor) = _gpsInfo(status.rtkState);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        _Chip(
          icon: Icons.battery_charging_full_rounded,
          label: status.batteryPercent != null
              ? '${status.batteryPercent}%'
              : '--',
          color: _batteryColor(status.batteryPercent),
        ),
        const SizedBox(height: 4),
        _Chip(
          icon: Icons.gps_fixed_rounded,
          label: gpsLabel,
          color: gpsColor,
        ),
        const SizedBox(height: 4),
        _Chip(
          icon: Icons.directions_walk_rounded,
          label: _phaseLabel(status.statePhase ?? 'idle'),
        ),
      ],
    );
  }

  static Color _batteryColor(int? percent) {
    if (percent == null) return const Color(0xFFCBD5E1);
    if (percent <= 20) return const Color(0xFFEF4444);
    if (percent <= 40) return const Color(0xFFF97316);
    if (percent <= 60) return const Color(0xFFFACC15);
    return const Color(0xFF4ADE80);
  }

  static (String, Color) _gpsInfo(String? rtkState) {
    return switch (rtkState) {
      'RTK Fixed' => ('GPS \u2713', const Color(0xFF4ADE80)),
      'RTK Float' => ('GPS ~', const Color(0xFFFACC15)),
      'Single' => ('GPS schwach', const Color(0xFFF97316)),
      'No Fix' => ('Kein GPS', const Color(0xFFEF4444)),
      null => ('GPS --', const Color(0xFF94A3B8)),
      _ => (rtkState, const Color(0xFF94A3B8)),
    };
  }

  static String _phaseLabel(String phase) {
    return switch (phase) {
      'mowing' => 'Mäht',
      'docking' => 'Dockt',
      'undocking' => 'Undockt',
      'navigating_to_start' => 'Fährt los',
      'charging' => 'Lädt',
      'obstacle_recovery' => 'Hindernis',
      'gps_recovery' => 'GPS-Warte',
      'waiting_for_rain' => 'Regen-Warte',
      'fault' => 'Fehler',
      _ => 'Bereit',
    };
  }
}

class _Chip extends StatelessWidget {
  const _Chip({required this.icon, required this.label, this.color});
  final IconData icon;
  final String label;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final effectiveColor = color ?? const Color(0xFFCBD5E1);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: const Color(0xCC0F172A),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: const Color(0x44FFFFFF)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Icon(icon, size: 13, color: effectiveColor),
          const SizedBox(width: 5),
          Text(label,
              style: TextStyle(
                  color: effectiveColor,
                  fontSize: 12,
                  fontWeight: FontWeight.w500)),
        ],
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Running state bar (shown when mowing/docking/etc.)
// ---------------------------------------------------------------------------

class _RunningStateBar extends StatelessWidget {
  const _RunningStateBar({required this.status, this.missionName});
  final RobotStatus status;
  final String? missionName;

  static const Map<String, Color> _phaseColors = <String, Color>{
    'mowing': Color(0xFF2563EB),
    'docking': Color(0xFFD97706),
    'undocking': Color(0xFF7C3AED),
    'navigating_to_start': Color(0xFF0891B2),
    'obstacle_recovery': Color(0xFFDC2626),
    'gps_recovery': Color(0xFFCA8A04),
    'waiting_for_rain': Color(0xFF0284C7),
  };

  static const Map<String, String> _phaseLabels = <String, String>{
    'mowing': 'Mäht',
    'docking': 'Fährt zur Ladestation',
    'undocking': 'Verlässt Ladestation',
    'navigating_to_start': 'Fährt zur Startposition',
    'obstacle_recovery': 'Hindernis-Ausweichen',
    'gps_recovery': 'Warte auf GPS',
    'waiting_for_rain': 'Warte auf trockenes Wetter',
  };

  @override
  Widget build(BuildContext context) {
    final phase = status.statePhase ?? 'idle';
    final color = _phaseColors[phase] ?? const Color(0xFF2563EB);
    final label = _phaseLabels[phase] ?? phase;

    return Container(
      color: const Color(0xFF0A1020),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          LinearProgressIndicator(
            value: null,
            backgroundColor: const Color(0xFF1E3A5F),
            valueColor: AlwaysStoppedAnimation<Color>(color),
            minHeight: 3,
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(12, 8, 12, 8),
            child: Row(
              children: <Widget>[
                Container(
                  width: 8,
                  height: 8,
                  decoration:
                      BoxDecoration(color: color, shape: BoxShape.circle),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(label,
                      style: TextStyle(
                          color: color,
                          fontSize: 13,
                          fontWeight: FontWeight.w600)),
                ),
                if (status.batteryPercent != null)
                  Row(
                    children: <Widget>[
                      const Icon(Icons.battery_charging_full_rounded,
                          size: 14, color: Color(0xFF94A3B8)),
                      const SizedBox(width: 4),
                      Text('${status.batteryPercent}%',
                          style: const TextStyle(
                              color: Color(0xFF94A3B8), fontSize: 12)),
                    ],
                  ),
                if (status.mowDistanceM != null) ...<Widget>[
                  const SizedBox(width: 12),
                  const Icon(Icons.straighten_rounded,
                      size: 14, color: Color(0xFF94A3B8)),
                  const SizedBox(width: 4),
                  Text(
                    '${status.mowDistanceM!.toStringAsFixed(0)} m',
                    style:
                        const TextStyle(color: Color(0xFF94A3B8), fontSize: 12),
                  ),
                ],
                if (status.mowDurationSec != null) ...<Widget>[
                  const SizedBox(width: 12),
                  const Icon(Icons.timer_outlined,
                      size: 14, color: Color(0xFF94A3B8)),
                  const SizedBox(width: 4),
                  Text(
                    _formatDuration(status.mowDurationSec!),
                    style:
                        const TextStyle(color: Color(0xFF94A3B8), fontSize: 12),
                  ),
                ],
              ],
            ),
          ),
          if (missionName != null)
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
              child: Row(
                children: <Widget>[
                  const Icon(Icons.route_rounded,
                      size: 13, color: Color(0xFF64748B)),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(missionName!,
                        style: const TextStyle(
                            color: Color(0xFF64748B), fontSize: 12),
                        overflow: TextOverflow.ellipsis),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }

  static String _formatDuration(int seconds) {
    final h = seconds ~/ 3600;
    final m = (seconds % 3600) ~/ 60;
    if (h > 0) return '${h}h ${m.toString().padLeft(2, '0')}m';
    return '${m}m';
  }
}

// ---------------------------------------------------------------------------
// Ready panel (shown when idle/charging and connected)
// ---------------------------------------------------------------------------

class _ReadyPanel extends StatelessWidget {
  const _ReadyPanel({
    required this.status,
    required this.isConnected,
    required this.hasFault,
    required this.isPaused,
    required this.hasPerimeter,
    required this.hasDock,
    required this.hasZones,
    required this.hasMission,
    required this.hasRecurringSchedule,
    required this.hasRememberedRobot,
    required this.onGoMapSetup,
    required this.onGoMapEdit,
    required this.onGoMissions,
    required this.onGoService,
    required this.onGoStatistics,
    required this.onReconnect,
    required this.onResume,
  });

  final RobotStatus status;
  final bool isConnected;
  final bool hasFault;
  final bool isPaused;
  final bool hasPerimeter;
  final bool hasDock;
  final bool hasZones;
  final bool hasMission;
  final bool hasRecurringSchedule;
  final bool hasRememberedRobot;
  final VoidCallback onGoMapSetup;
  final VoidCallback onGoMapEdit;
  final VoidCallback onGoMissions;
  final VoidCallback onGoService;
  final VoidCallback onGoStatistics;
  final VoidCallback onReconnect;
  final VoidCallback? onResume;

  @override
  Widget build(BuildContext context) {
    final hasMapReady = hasPerimeter && hasDock;
    final stateKind = !isConnected
        ? (hasRememberedRobot
            ? _DashboardStateKind.offline
            : _DashboardStateKind.noRobot)
        : hasFault
            ? _DashboardStateKind.fault
            : isPaused
                ? _DashboardStateKind.paused
                : !hasMapReady
                    ? _DashboardStateKind.noMap
                    : !hasZones
                        ? _DashboardStateKind.noZones
                        : !hasRecurringSchedule
                            ? _DashboardStateKind.noSchedule
                            : _DashboardStateKind.ready;

    final title = switch (stateKind) {
      _DashboardStateKind.noRobot => 'Roboter zuerst verbinden',
      _DashboardStateKind.offline => 'Roboter ist nicht verbunden',
      _DashboardStateKind.fault => 'Fehler braucht Aufmerksamkeit',
      _DashboardStateKind.paused => 'Mähvorgang ist pausiert',
      _DashboardStateKind.noMap => 'Es ist noch keine Karte angelegt.',
      _DashboardStateKind.noZones => 'Zonen fehlen noch',
      _DashboardStateKind.noSchedule => 'Automatik ist noch nicht geplant',
      _DashboardStateKind.ready => 'Bereit für den nächsten Einsatz',
    };
    final subtitle = switch (stateKind) {
      _DashboardStateKind.noRobot => 'Starte mit Robotersuche oder gib Alfred manuell an, damit Dashboard und Karte lebendig werden.',
      _DashboardStateKind.offline => 'Verbinde dich erneut mit Alfred, bevor du Karte oder Mission steuerst.',
      _DashboardStateKind.fault => status.lastError ?? 'Ein Fehler blockiert den normalen Betrieb. Prüfe Status und Service.',
      _DashboardStateKind.paused => 'Der aktuelle Lauf steht still. Du kannst fortsetzen, stoppen oder Alfred zur Station schicken.',
      _DashboardStateKind.noMap => 'Bitte zuerst Perimeter, No-Go-Bereiche und Docking festlegen.',
      _DashboardStateKind.noZones => 'Die Basiskarte ist da. Jetzt fehlen noch Mähzonen für Planung und Betrieb.',
      _DashboardStateKind.noSchedule => 'Manueller Start ist möglich. Für Automatik fehlt noch mindestens ein wiederkehrender Zeitplan.',
      _DashboardStateKind.ready => 'Karte, Zonen und Planung sind vorhanden. Du kannst direkt starten oder Details öffnen.',
    };
    final primaryAction = switch (stateKind) {
      _DashboardStateKind.noRobot => onReconnect,
      _DashboardStateKind.offline => onReconnect,
      _DashboardStateKind.fault => onGoService,
      _DashboardStateKind.paused => onResume ?? onGoMissions,
      _DashboardStateKind.noMap => onGoMapSetup,
      _DashboardStateKind.noZones => onGoMapEdit,
      _DashboardStateKind.noSchedule => onGoMissions,
      _DashboardStateKind.ready => onGoMissions,
    };
    final primaryIcon = switch (stateKind) {
      _DashboardStateKind.noRobot => Icons.add_link_rounded,
      _DashboardStateKind.offline => Icons.wifi_find_rounded,
      _DashboardStateKind.fault => Icons.miscellaneous_services_outlined,
      _DashboardStateKind.paused => Icons.play_circle_outline_rounded,
      _DashboardStateKind.noMap => Icons.map_outlined,
      _DashboardStateKind.noZones => Icons.edit_location_alt_outlined,
      _DashboardStateKind.noSchedule => Icons.schedule_outlined,
      _DashboardStateKind.ready => Icons.route_outlined,
    };
    final primaryLabel = switch (stateKind) {
      _DashboardStateKind.noRobot => 'Roboter verbinden',
      _DashboardStateKind.offline => 'Neu verbinden',
      _DashboardStateKind.fault => 'Service öffnen',
      _DashboardStateKind.paused => onResume == null ? 'Mission öffnen' : 'Fortsetzen',
      _DashboardStateKind.noMap => 'Karte anlegen',
      _DashboardStateKind.noZones => 'Zonen anlegen',
      _DashboardStateKind.noSchedule => 'Zeitplan anlegen',
      _DashboardStateKind.ready => 'Mission öffnen',
    };

    return Container(
      color: const Color(0xFF0A1020),
      padding: const EdgeInsets.fromLTRB(12, 10, 12, 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            title,
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
          ),
          const SizedBox(height: 4),
          Text(
            subtitle,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: const Color(0xFF94A3B8),
                ),
          ),
          const SizedBox(height: 12),
          Row(
            children: <Widget>[
              _CheckItem(done: isConnected, label: 'Verbunden', onFix: isConnected ? null : onReconnect),
              const SizedBox(width: 8),
              _CheckItem(
                done: hasPerimeter,
                label: 'Perimeter',
                onFix: hasPerimeter ? null : onGoMapSetup,
              ),
              const SizedBox(width: 8),
              _CheckItem(
                done: hasDock,
                label: 'Docking',
                onFix: hasDock ? null : onGoMapSetup,
              ),
              const SizedBox(width: 8),
              _CheckItem(
                done: hasZones,
                label: 'Zonen',
                onFix: hasZones ? null : onGoMapEdit,
              ),
              const SizedBox(width: 8),
              _CheckItem(
                done: hasMission,
                label: 'Mission',
                onFix: hasMission ? null : onGoMissions,
              ),
            ],
          ),
          const SizedBox(height: 8),
          _StatePillRow(
            stateKind: stateKind,
            hasRecurringSchedule: hasRecurringSchedule,
          ),
          const SizedBox(height: 12),
          Row(
            children: <Widget>[
              Expanded(
                child: FilledButton.tonalIcon(
                  onPressed: primaryAction,
                  icon: Icon(primaryIcon),
                  label: Text(primaryLabel),
                ),
              ),
              const SizedBox(width: 8),
              IconButton.filledTonal(
                tooltip: 'Statistik',
                onPressed: onGoStatistics,
                icon: const Icon(Icons.insights_outlined),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

enum _DashboardStateKind {
  noRobot,
  offline,
  fault,
  paused,
  noMap,
  noZones,
  noSchedule,
  ready,
}

class _StatePillRow extends StatelessWidget {
  const _StatePillRow({
    required this.stateKind,
    required this.hasRecurringSchedule,
  });

  final _DashboardStateKind stateKind;
  final bool hasRecurringSchedule;

  @override
  Widget build(BuildContext context) {
    final pills = <String>[
      switch (stateKind) {
        _DashboardStateKind.noRobot => 'Nicht verbunden',
        _DashboardStateKind.offline => 'Offline',
        _DashboardStateKind.fault => 'Fehler',
        _DashboardStateKind.paused => 'Pausiert',
        _DashboardStateKind.noMap => 'Setup',
        _DashboardStateKind.noZones => 'Zonen fehlen',
        _DashboardStateKind.noSchedule => 'Manuell',
        _DashboardStateKind.ready => 'Betriebsbereit',
      },
      hasRecurringSchedule ? 'Automatik aktiv' : 'Kein Zeitplan',
    ];

    return Wrap(
      spacing: 8,
      runSpacing: 8,
      children: pills
          .map(
            (label) => Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              decoration: BoxDecoration(
                color: const Color(0xFF111B2E),
                borderRadius: BorderRadius.circular(999),
              ),
              child: Text(
                label,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFFBFDBFE),
                      fontWeight: FontWeight.w600,
                    ),
              ),
            ),
          )
          .toList(growable: false),
    );
  }
}

class _CheckItem extends StatelessWidget {
  const _CheckItem({required this.done, required this.label, this.onFix});
  final bool done;
  final String label;
  final VoidCallback? onFix;

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: GestureDetector(
        onTap: onFix,
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 6, horizontal: 6),
          decoration: BoxDecoration(
            color: done ? const Color(0xFF052E16) : const Color(0xFF1C0A0A),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(
              color: done ? const Color(0xFF166534) : const Color(0xFF7F1D1D),
            ),
          ),
          child: Column(
            children: <Widget>[
              Icon(
                done
                    ? Icons.check_circle_rounded
                    : Icons.radio_button_unchecked_rounded,
                size: 16,
                color: done ? const Color(0xFF4ADE80) : const Color(0xFFFCA5A5),
              ),
              const SizedBox(height: 2),
              Text(
                label,
                style: TextStyle(
                  fontSize: 12,
                  color:
                      done ? const Color(0xFF4ADE80) : const Color(0xFFFCA5A5),
                  fontWeight: FontWeight.w500,
                ),
                textAlign: TextAlign.center,
                overflow: TextOverflow.ellipsis,
              ),
              if (!done)
                const Text('→',
                    style: TextStyle(fontSize: 12, color: Color(0xFFFCA5A5))),
            ],
          ),
        ),
      ),
    );
  }
}

class _QuickActionTile extends StatelessWidget {
  const _QuickActionTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return ListTile(
      contentPadding: EdgeInsets.zero,
      leading: CircleAvatar(
        backgroundColor: const Color(0x1F2563EB),
        foregroundColor: const Color(0xFF60A5FA),
        child: Icon(icon),
      ),
      title: Text(title),
      subtitle: Text(subtitle),
      trailing: const Icon(Icons.chevron_right_rounded),
      onTap: onTap,
    );
  }
}

// ---------------------------------------------------------------------------
// Virtual joystick overlay
// ---------------------------------------------------------------------------

class _ManualDriveMode extends StatefulWidget {
  const _ManualDriveMode({
    required this.status,
    required this.onDriveChanged,
    required this.onClose,
    required this.onEmergencyStop,
  });

  final RobotStatus status;
  final void Function(double linear, double angular) onDriveChanged;
  final VoidCallback onClose;
  final VoidCallback? onEmergencyStop;

  @override
  State<_ManualDriveMode> createState() => _ManualDriveModeState();
}

class _ManualDriveModeState extends State<_ManualDriveMode> {
  static const Duration _sendInterval = Duration(milliseconds: 80);

  double _linear = 0;
  double _angular = 0;
  Timer? _sendTimer;

  @override
  void dispose() {
    _sendTimer?.cancel();
    widget.onDriveChanged(0, 0);
    super.dispose();
  }

  void _updateLinear(double value) {
    setState(() => _linear = value);
    _syncDriveLoop();
    widget.onDriveChanged(_linear, _angular);
  }

  void _updateAngular(double value) {
    setState(() => _angular = value);
    _syncDriveLoop();
    widget.onDriveChanged(_linear, _angular);
  }

  void _syncDriveLoop() {
    final active = _linear.abs() > 0.001 || _angular.abs() > 0.001;
    if (active) {
      _sendTimer ??= Timer.periodic(_sendInterval, (_) {
        widget.onDriveChanged(_linear, _angular);
      });
      return;
    }
    _sendTimer?.cancel();
    _sendTimer = null;
  }

  @override
  Widget build(BuildContext context) {
    final isLandscape = MediaQuery.of(context).orientation == Orientation.landscape;

    return Container(
      decoration: const BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: <Color>[
            Color(0x8809111F),
            Color(0x2209111F),
            Color(0x6609111F),
          ],
        ),
      ),
      child: SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(18, 12, 18, 14),
          child: Column(
            children: <Widget>[
              Row(
                children: <Widget>[
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                    decoration: BoxDecoration(
                      color: const Color(0xC20F172A),
                      borderRadius: BorderRadius.circular(999),
                      border: Border.all(color: const Color(0x33FFFFFF)),
                    ),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: <Widget>[
                        const Icon(Icons.gamepad_rounded,
                            size: 16, color: Colors.white),
                        const SizedBox(width: 8),
                        Text(
                          'Manueller Modus',
                          style: Theme.of(context).textTheme.labelLarge?.copyWith(
                                color: Colors.white,
                                fontWeight: FontWeight.w700,
                              ),
                        ),
                      ],
                    ),
                  ),
                  const Spacer(),
                  if (widget.onEmergencyStop != null)
                    FilledButton.icon(
                      onPressed: widget.onEmergencyStop,
                      icon: const Icon(Icons.stop_circle_outlined),
                      style: FilledButton.styleFrom(
                        backgroundColor: const Color(0xFFDC2626),
                        foregroundColor: Colors.white,
                      ),
                      label: const Text('Not-Aus'),
                    ),
                  const SizedBox(width: 12),
                  IconButton.filledTonal(
                    tooltip: 'Manuellen Modus verlassen',
                    onPressed: widget.onClose,
                    icon: const Icon(Icons.close_rounded),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              if (!isLandscape)
                Container(
                  width: double.infinity,
                  padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
                  decoration: BoxDecoration(
                    color: const Color(0xC20F172A),
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(color: const Color(0x33FFFFFF)),
                  ),
                  child: Text(
                    'Bitte Gerät ins Querformat drehen.',
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                          color: Colors.white,
                          fontWeight: FontWeight.w600,
                        ),
                  ),
                ),
              const Spacer(),
              Row(
                crossAxisAlignment: CrossAxisAlignment.end,
                children: <Widget>[
                  Expanded(
                    child: _AxisJoystick(
                      title: 'Lenkung',
                      subtitle: 'Links / Rechts',
                      axis: _JoystickAxis.horizontal,
                      accentColor: const Color(0xFF38BDF8),
                      onChanged: (value) => _updateAngular(-value),
                    ),
                  ),
                  const SizedBox(width: 24),
                  Expanded(
                    child: _AxisJoystick(
                      title: 'Fahrt',
                      subtitle: 'Vor / Zurück',
                      axis: _JoystickAxis.vertical,
                      accentColor: const Color(0xFF22C55E),
                      onChanged: _updateLinear,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                'Links lenken, rechts fahren.',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFFE2E8F0),
                      fontWeight: FontWeight.w600,
                    ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

enum _JoystickAxis { horizontal, vertical }

class _AxisJoystick extends StatefulWidget {
  const _AxisJoystick({
    required this.title,
    required this.subtitle,
    required this.axis,
    required this.accentColor,
    required this.onChanged,
  });

  final String title;
  final String subtitle;
  final _JoystickAxis axis;
  final Color accentColor;
  final ValueChanged<double> onChanged;

  @override
  State<_AxisJoystick> createState() => _AxisJoystickState();
}

class _AxisJoystickState extends State<_AxisJoystick> {
  static const double _joystickRadius = 72.0;
  static const double _thumbRadius = 22.0;

  Offset _thumbOffset = Offset.zero;

  void _onPanStart(DragStartDetails details) => _updateThumb(details.localPosition);

  void _onPanUpdate(DragUpdateDetails details) => _updateThumb(details.localPosition);

  void _onPanEnd(DragEndDetails details) {
    setState(() => _thumbOffset = Offset.zero);
    widget.onChanged(0);
  }

  void _updateThumb(Offset localPosition) {
    const center = Offset(_joystickRadius, _joystickRadius);
    final delta = localPosition - center;
    final constrained = switch (widget.axis) {
      _JoystickAxis.horizontal => Offset(
          delta.dx.clamp(-_joystickRadius, _joystickRadius),
          0,
        ),
      _JoystickAxis.vertical => Offset(
          0,
          delta.dy.clamp(-_joystickRadius, _joystickRadius),
        ),
    };
    setState(() => _thumbOffset = constrained);
    final normalized = switch (widget.axis) {
      _JoystickAxis.horizontal =>
        (constrained.dx / _joystickRadius).clamp(-1.0, 1.0),
      _JoystickAxis.vertical =>
        (-constrained.dy / _joystickRadius).clamp(-1.0, 1.0),
    };
    widget.onChanged(normalized);
  }

  @override
  Widget build(BuildContext context) {
    const diameter = _joystickRadius * 2;
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        Text(
          widget.title,
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                color: Colors.white,
                fontWeight: FontWeight.w700,
              ),
        ),
        const SizedBox(height: 2),
        Text(
          widget.subtitle,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: const Color(0xFFCBD5E1),
              ),
        ),
        const SizedBox(height: 16),
        GestureDetector(
          onPanStart: _onPanStart,
          onPanUpdate: _onPanUpdate,
          onPanEnd: _onPanEnd,
          child: SizedBox(
            width: diameter,
            height: diameter,
            child: CustomPaint(
              painter: _JoystickPainter(
                thumbOffset: _thumbOffset,
                joystickRadius: _joystickRadius,
                thumbRadius: _thumbRadius,
                accentColor: widget.accentColor,
                axis: widget.axis,
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _JoystickPainter extends CustomPainter {
  const _JoystickPainter({
    required this.thumbOffset,
    required this.joystickRadius,
    required this.thumbRadius,
    required this.accentColor,
    required this.axis,
  });

  final Offset thumbOffset;
  final double joystickRadius;
  final double thumbRadius;
  final Color accentColor;
  final _JoystickAxis axis;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    canvas.drawCircle(
        center,
        joystickRadius,
        Paint()
          ..color = accentColor.withValues(alpha: 0.16)
          ..style = PaintingStyle.fill);
    canvas.drawCircle(
        center,
        joystickRadius,
        Paint()
          ..color = accentColor
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2);
    canvas.drawCircle(center, 6, Paint()..color = accentColor.withValues(alpha: 0.4));
    final lp = Paint()
      ..color = accentColor.withValues(alpha: 0.3)
      ..strokeWidth = 1;
    if (axis == _JoystickAxis.horizontal) {
      canvas.drawLine(Offset(center.dx - joystickRadius, center.dy),
          Offset(center.dx + joystickRadius, center.dy), lp);
    } else {
      canvas.drawLine(Offset(center.dx, center.dy - joystickRadius),
          Offset(center.dx, center.dy + joystickRadius), lp);
    }
    final tc = center + thumbOffset;
    canvas.drawCircle(tc, thumbRadius, Paint()..color = accentColor);
    canvas.drawCircle(
        tc,
        thumbRadius,
        Paint()
          ..color = Colors.white.withValues(alpha: 0.7)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2);
    canvas.drawCircle(
        tc, thumbRadius * 0.35, Paint()..color = Colors.white.withValues(alpha: 0.7));
  }

  @override
  bool shouldRepaint(covariant _JoystickPainter old) =>
      old.thumbOffset != thumbOffset;
}
