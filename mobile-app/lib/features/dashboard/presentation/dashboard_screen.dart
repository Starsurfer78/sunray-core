import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/mission/mission.dart';
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
  bool _joystickVisible = false;
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

  @override
  Widget build(BuildContext context) {
    final rememberedRobotAsync = ref.watch(rememberedRobotProvider);
    final missions = ref.watch(missionsProvider);
    final selectedMissionId = ref.watch(selectedMissionIdProvider);
    final mission = missions.cast<Mission?>().firstWhere(
        (entry) => entry?.id == selectedMissionId,
        orElse: () => missions.isEmpty ? null : missions.first,
      );
    final map = ref.watch(mapGeometryProvider);
    final status = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
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

    return Scaffold(
      appBar: AppBar(
        title: Text(activeRobot?.name ?? 'Dashboard'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Schnellzugriffe',
            onPressed: () => _openQuickActions(context),
            icon: const Icon(Icons.tune_rounded),
          ),
          PopupMenuButton<_MenuAction>(
            onSelected: (action) {
              switch (action) {
                case _MenuAction.switchRobot:
                  _handleSwitchRobot();
                case _MenuAction.disconnect:
                  _handleDisconnect();
              }
            },
            itemBuilder: (context) => const <PopupMenuEntry<_MenuAction>>[
              PopupMenuItem(
                  value: _MenuAction.switchRobot,
                  child: Text('Roboter wechseln')),
              PopupMenuItem(
                  value: _MenuAction.disconnect, child: Text('Trennen')),
            ],
          ),
        ],
      ),
      body: Column(
        children: <Widget>[
          // Map
          Expanded(
            flex: 3,
            child: Stack(
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
                Positioned(
                  top: 10,
                  left: 10,
                  child: _StatusOverlay(status: status),
                ),
                Positioned(
                  bottom: 14,
                  right: 14,
                  child: FloatingActionButton(
                    heroTag: 'joystick_fab',
                    tooltip: 'Virtuelles Steuerkreuz',
                    mini: true,
                    onPressed: isConnected ? _openJoystick : null,
                    backgroundColor: isConnected
                        ? const Color(0xFF1E40AF)
                        : const Color(0xFF334155),
                    child: const Icon(Icons.gamepad_rounded, size: 20),
                  ),
                ),
              ],
            ),
          ),
          // Connection notice
          if (hasConnectionNotice)
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 6, 12, 0),
              child: ConnectionNotice(status: status),
            ),
          // Running state bar OR checklist
          if (isConnected && isActive)
            _RunningStateBar(status: status, missionName: mission?.name)
          else
            _ReadyPanel(
              status: status,
              isConnected: isConnected,
              hasFault: hasFault,
              isPaused: isPaused,
              hasPerimeter: map.perimeter.length >= 3,
              hasDock: map.dock.isNotEmpty,
              hasZones: hasZones,
              hasMission: missions.isNotEmpty,
              hasRecurringSchedule: hasRecurringSchedule,
              hasRememberedRobot: rememberedRobotAsync.valueOrNull != null,
              onGoMapSetup: () => context.push('/map/setup'),
              onGoMapEdit: () => context.push('/map/edit'),
              onGoMissions: () => context.push('/missions'),
              onGoService: () => context.push('/service'),
              onGoStatistics: () => context.push('/statistics'),
              onReconnect: _handleConnectOrSwitch,
              onResume: mission == null
                  ? null
                  : () => _runAction(() {
                        ref
                            .read(robotConnectionControllerProvider)
                            .startMowing(missionId: mission.id);
                      }),
            ),
          if (isConnected && mission != null)
            _MissionFocusCard(
              missions: missions,
              mission: mission,
              selectedMissionId: selectedMissionId,
              map: map,
              selectedZoneId: selectedZoneId,
              onMissionSelected: (missionId) {
                ref.read(selectedMissionIdProvider.notifier).state = missionId;
                setState(() => _selectedZoneId = null);
              },
              onZoneSelected: (zoneId) {
                setState(() {
                  _selectedZoneId = _selectedZoneId == zoneId ? null : zoneId;
                });
              },
              onOpenDetails: () => _openMissionDetails(
                context,
                mission,
                map,
                selectedZoneId,
              ),
              onOpenPlanning: () => context.push('/missions'),
            ),
          // Bottom action bar
          _BottomActionBar(
            isConnected: isConnected,
            busy: _commandBusy,
            missionName: mission?.name,
            isActive: isActive,
            isPaused: isPaused,
            onMow: mission == null
                ? null
                : () => _runAction(() {
                      ref
                          .read(robotConnectionControllerProvider)
                          .startMowing(missionId: mission.id);
                    }),
            onStop: () => _runAction(() =>
                ref.read(robotConnectionControllerProvider).emergencyStop()),
            onDock: () => _runAction(() =>
                ref.read(robotConnectionControllerProvider).startDocking()),
          ),
          if (_joystickVisible)
            _JoystickOverlay(
              onDrive: (vx, vy) => ref
                  .read(robotConnectionControllerProvider)
                  .sendDriveCommand(vy, -vx),
              onClose: () {
                ref
                    .read(robotConnectionControllerProvider)
                    .sendDriveCommand(0, 0);
                setState(() => _joystickVisible = false);
              },
            ),
        ],
      ),
    );
  }

  void _openJoystick() => setState(() => _joystickVisible = true);

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
                subtitle: 'App- und Roboterdetails als sekundaeren Bereich oeffnen',
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
                subtitle: 'Updates, Diagnose und Systemfunktionen oeffnen',
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
}

enum _MenuAction { switchRobot, disconnect }

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
      _DashboardStateKind.paused => 'Maehvorgang ist pausiert',
      _DashboardStateKind.noMap => 'Kartensetup abschliessen',
      _DashboardStateKind.noZones => 'Zonen fehlen noch',
      _DashboardStateKind.noSchedule => 'Automatik ist noch nicht geplant',
      _DashboardStateKind.ready => 'Bereit fuer den naechsten Einsatz',
    };
    final subtitle = switch (stateKind) {
      _DashboardStateKind.noRobot => 'Starte mit Discovery oder gib Alfred manuell an, damit Dashboard und Karte lebendig werden.',
      _DashboardStateKind.offline => 'Verbinde dich erneut mit Alfred, bevor du Karte oder Mission steuerst.',
      _DashboardStateKind.fault => status.lastError ?? 'Ein Fehler blockiert den normalen Betrieb. Pruefe Status und Service.',
      _DashboardStateKind.paused => 'Der aktuelle Lauf steht still. Du kannst fortsetzen, stoppen oder Alfred zur Station schicken.',
      _DashboardStateKind.noMap => 'Perimeter und Dock sind die Basis fuer jede saubere Route.',
      _DashboardStateKind.noZones => 'Die Basiskarte ist da. Jetzt fehlen noch Mähzonen fuer Planung und Betrieb.',
      _DashboardStateKind.noSchedule => 'Manueller Start ist moeglich. Fuer Automatik fehlt noch mindestens ein wiederkehrender Zeitplan.',
      _DashboardStateKind.ready => 'Karte, Zonen und Planung sind vorhanden. Du kannst direkt starten oder Details oeffnen.',
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
                label: 'Dock',
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
                  onPressed: switch (stateKind) {
                    _DashboardStateKind.noRobot => onReconnect,
                    _DashboardStateKind.offline => onReconnect,
                    _DashboardStateKind.fault => onGoStatistics,
                    _DashboardStateKind.paused => onResume ?? onGoStatistics,
                    _DashboardStateKind.noMap => onGoMapSetup,
                    _DashboardStateKind.noZones => onGoMapEdit,
                    _DashboardStateKind.noSchedule => onGoMissions,
                    _DashboardStateKind.ready => onGoMapEdit,
                  },
                  icon: Icon(switch (stateKind) {
                    _DashboardStateKind.noRobot => Icons.add_link_rounded,
                    _DashboardStateKind.offline => Icons.wifi_find_rounded,
                    _DashboardStateKind.fault => Icons.warning_amber_rounded,
                    _DashboardStateKind.paused => Icons.play_circle_outline_rounded,
                    _DashboardStateKind.noMap => Icons.map_outlined,
                    _DashboardStateKind.noZones => Icons.edit_location_alt_outlined,
                    _DashboardStateKind.noSchedule => Icons.schedule_outlined,
                    _DashboardStateKind.ready => Icons.map_outlined,
                  }),
                  label: Text(switch (stateKind) {
                    _DashboardStateKind.noRobot => 'Roboter verbinden',
                    _DashboardStateKind.offline => 'Neu verbinden',
                    _DashboardStateKind.fault => 'Fehler pruefen',
                    _DashboardStateKind.paused => onResume == null ? 'Verlauf ansehen' : 'Fortsetzen',
                    _DashboardStateKind.noMap => 'Karte anlegen',
                    _DashboardStateKind.noZones => 'Zonen anlegen',
                    _DashboardStateKind.noSchedule => 'Zeitplan anlegen',
                    _DashboardStateKind.ready => 'Karte pruefen',
                  }),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: FilledButton.tonalIcon(
                  onPressed: switch (stateKind) {
                    _DashboardStateKind.noRobot => onReconnect,
                    _DashboardStateKind.offline => onGoStatistics,
                    _DashboardStateKind.fault => onGoService,
                    _DashboardStateKind.paused => onGoMissions,
                    _DashboardStateKind.noMap => onGoMissions,
                    _DashboardStateKind.noZones => onGoMissions,
                    _DashboardStateKind.noSchedule => onGoMissions,
                    _DashboardStateKind.ready => onGoMissions,
                  },
                  icon: Icon(switch (stateKind) {
                    _DashboardStateKind.noRobot => Icons.radar_rounded,
                    _DashboardStateKind.offline => Icons.route_outlined,
                    _DashboardStateKind.fault => Icons.miscellaneous_services_outlined,
                    _DashboardStateKind.paused => Icons.route_outlined,
                    _DashboardStateKind.noMap => Icons.route_outlined,
                    _DashboardStateKind.noZones => Icons.route_outlined,
                    _DashboardStateKind.noSchedule => Icons.route_outlined,
                    _DashboardStateKind.ready => Icons.route_outlined,
                  }),
                  label: Text(switch (stateKind) {
                    _DashboardStateKind.noRobot => 'Discovery öffnen',
                    _DashboardStateKind.offline => 'Status',
                    _DashboardStateKind.fault => 'Service öffnen',
                    _DashboardStateKind.paused => 'Planung oeffnen',
                    _DashboardStateKind.noMap => 'Planung oeffnen',
                    _DashboardStateKind.noZones => 'Planung oeffnen',
                    _DashboardStateKind.noSchedule => 'Planung oeffnen',
                    _DashboardStateKind.ready => 'Planung oeffnen',
                  }),
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
// Bottom action bar
// ---------------------------------------------------------------------------

class _BottomActionBar extends StatelessWidget {
  const _BottomActionBar({
    required this.isConnected,
    required this.busy,
    required this.missionName,
    required this.isActive,
    required this.isPaused,
    required this.onMow,
    required this.onStop,
    required this.onDock,
  });

  final bool isConnected;
  final bool busy;
  final String? missionName;
  final bool isActive;
  final bool isPaused;
  final VoidCallback? onMow;
  final VoidCallback onStop;
  final VoidCallback onDock;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return SafeArea(
      top: false,
      child: Container(
        decoration: BoxDecoration(
          color: theme.cardColor,
          border: Border(top: BorderSide(color: theme.dividerColor)),
        ),
        padding: const EdgeInsets.fromLTRB(12, 10, 12, 10),
        child: Row(
          children: <Widget>[
            if (!isActive)
              Expanded(
                child: FilledButton.icon(
                  onPressed: isConnected && !busy ? onMow : null,
                  icon: const Icon(Icons.play_arrow_rounded, size: 18),
                  label: Text(
                    missionName == null
                        ? 'Keine Mission'
                        : isPaused
                            ? 'Fortsetzen'
                            : 'Mähen',
                  ),
                  style: FilledButton.styleFrom(
                      padding: const EdgeInsets.symmetric(vertical: 10)),
                ),
              ),
            if (!isActive) const SizedBox(width: 8),
            Expanded(
              child: FilledButton.icon(
                onPressed: isConnected && !busy ? onStop : null,
                icon: const Icon(Icons.stop_rounded, size: 18),
                label: const Text('Stop'),
                style: FilledButton.styleFrom(
                  backgroundColor: const Color(0xFFB91C1C),
                  foregroundColor: Colors.white,
                  disabledBackgroundColor: const Color(0xFF7F1D1D),
                  disabledForegroundColor: const Color(0xFF9CA3AF),
                  padding: const EdgeInsets.symmetric(vertical: 10),
                ),
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: OutlinedButton.icon(
                onPressed: isConnected && !busy ? onDock : null,
                icon: const Icon(Icons.home_rounded, size: 18),
                label: const Text('Dock'),
                style: OutlinedButton.styleFrom(
                    padding: const EdgeInsets.symmetric(vertical: 10)),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Virtual joystick overlay
// ---------------------------------------------------------------------------

class _JoystickOverlay extends StatefulWidget {
  const _JoystickOverlay({required this.onDrive, required this.onClose});
  final void Function(double vx, double vy) onDrive;
  final VoidCallback onClose;

  @override
  State<_JoystickOverlay> createState() => _JoystickOverlayState();
}

class _JoystickOverlayState extends State<_JoystickOverlay> {
  static const double _joystickRadius = 80.0;
  static const double _thumbRadius = 24.0;
  static const Duration _sendInterval = Duration(milliseconds: 80);

  Offset _thumbOffset = Offset.zero;
  Timer? _sendTimer;

  @override
  void dispose() {
    _sendTimer?.cancel();
    super.dispose();
  }

  void _onPanStart(DragStartDetails d) {
    _updateThumb(d.localPosition);
    _sendTimer ??= Timer.periodic(_sendInterval, (_) => _emitCurrent());
  }

  void _onPanUpdate(DragUpdateDetails d) => _updateThumb(d.localPosition);

  void _onPanEnd(DragEndDetails d) {
    _sendTimer?.cancel();
    _sendTimer = null;
    setState(() => _thumbOffset = Offset.zero);
    widget.onDrive(0, 0);
  }

  void _emitCurrent() {
    final vx = (_thumbOffset.dx / _joystickRadius).clamp(-1.0, 1.0);
    final vy = (-_thumbOffset.dy / _joystickRadius).clamp(-1.0, 1.0);
    widget.onDrive(vx, vy);
  }

  void _updateThumb(Offset local) {
    const center = Offset(_joystickRadius, _joystickRadius);
    final delta = local - center;
    final dist = delta.distance;
    final clamped =
        dist <= _joystickRadius ? delta : delta / dist * _joystickRadius;
    setState(() => _thumbOffset = clamped);
    _emitCurrent();
  }

  @override
  Widget build(BuildContext context) {
    const diameter = _joystickRadius * 2;
    return Container(
      color: const Color(0xCC000000),
      child: SafeArea(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.end,
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.only(bottom: 32),
              child: Column(
                children: <Widget>[
                  const Text('Steuerkreuz',
                      style: TextStyle(
                          color: Color(0xFFCBD5E1),
                          fontSize: 14,
                          fontWeight: FontWeight.w600)),
                  const SizedBox(height: 20),
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
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  TextButton.icon(
                    onPressed: widget.onClose,
                    icon: const Icon(Icons.close_rounded,
                        color: Color(0xFF94A3B8)),
                    label: const Text('Schliessen',
                        style: TextStyle(color: Color(0xFF94A3B8))),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _JoystickPainter extends CustomPainter {
  const _JoystickPainter(
      {required this.thumbOffset,
      required this.joystickRadius,
      required this.thumbRadius});

  final Offset thumbOffset;
  final double joystickRadius;
  final double thumbRadius;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    canvas.drawCircle(
        center,
        joystickRadius,
        Paint()
          ..color = const Color(0x441E40AF)
          ..style = PaintingStyle.fill);
    canvas.drawCircle(
        center,
        joystickRadius,
        Paint()
          ..color = const Color(0xFF3B82F6)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2);
    canvas.drawCircle(center, 6, Paint()..color = const Color(0x664ADE80));
    final lp = Paint()
      ..color = const Color(0x443B82F6)
      ..strokeWidth = 1;
    canvas.drawLine(Offset(center.dx - joystickRadius, center.dy),
        Offset(center.dx + joystickRadius, center.dy), lp);
    canvas.drawLine(Offset(center.dx, center.dy - joystickRadius),
        Offset(center.dx, center.dy + joystickRadius), lp);
    final tc = center + thumbOffset;
    canvas.drawCircle(
        tc, thumbRadius, Paint()..color = const Color(0xFF2563EB));
    canvas.drawCircle(
        tc,
        thumbRadius,
        Paint()
          ..color = const Color(0xFF93C5FD)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2);
    canvas.drawCircle(
        tc, thumbRadius * 0.35, Paint()..color = const Color(0xAABAE6FD));
  }

  @override
  bool shouldRepaint(covariant _JoystickPainter old) =>
      old.thumbOffset != thumbOffset;
}
