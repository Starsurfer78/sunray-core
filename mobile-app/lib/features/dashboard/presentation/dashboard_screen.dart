import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/section_card.dart';

class DashboardScreen extends ConsumerStatefulWidget {
  const DashboardScreen({super.key});

  @override
  ConsumerState<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends ConsumerState<DashboardScreen> {
  bool _commandBusy = false;

  @override
  Widget build(BuildContext context) {
    final missions = ref.watch(missionsProvider);
    final mission = missions.isEmpty ? null : missions.first;
    final map = ref.watch(mapGeometryProvider);
    final status = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final isConnected = status.connectionState == ConnectionStateKind.connected;

    return Scaffold(
      appBar: AppBar(
        title: Text(activeRobot?.name ?? 'Dashboard'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Missionen',
            onPressed: () => context.go('/missions'),
            icon: const Icon(Icons.route_rounded),
          ),
          PopupMenuButton<_DashboardMenuAction>(
            onSelected: (action) {
              switch (action) {
                case _DashboardMenuAction.switchRobot:
                  _handleSwitchRobot();
                  break;
                case _DashboardMenuAction.disconnect:
                  _handleDisconnect();
                  break;
              }
            },
            itemBuilder: (context) => const <PopupMenuEntry<_DashboardMenuAction>>[
              PopupMenuItem(
                value: _DashboardMenuAction.switchRobot,
                child: Text('Roboter wechseln'),
              ),
              PopupMenuItem(
                value: _DashboardMenuAction.disconnect,
                child: Text('Trennen'),
              ),
            ],
          ),
          IconButton(
            tooltip: 'Service',
            onPressed: () => context.go('/service'),
            icon: const Icon(Icons.miscellaneous_services_rounded),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: <Widget>[
          ConnectionNotice(status: status),
          if (status.lastError != null &&
              (status.connectionState == ConnectionStateKind.error ||
                  status.connectionState == ConnectionStateKind.connecting))
            const SizedBox(height: 16),
          _StatusStrip(
            status: status,
            robotName: activeRobot?.name,
          ),
          const SizedBox(height: 16),
          SectionCard(
            title: 'Karte',
            child: _MiniMapCard(
              map: map,
              status: status,
            ),
          ),
          const SizedBox(height: 16),
          SectionCard(
            title: 'Schnellaktionen',
            child: _QuickActions(
              connected: isConnected,
              busy: _commandBusy,
              onStart: mission == null ? null : () => _runAction(() {
                    ref
                        .read(robotConnectionControllerProvider)
                        .startMowing(missionId: mission.id);
                  }),
              onStop: () => _runAction(() {
                    ref.read(robotConnectionControllerProvider).emergencyStop();
                  }),
              onDock: () => _runAction(() {
                    ref.read(robotConnectionControllerProvider).startDocking();
                  }),
            ),
          ),
          const SizedBox(height: 16),
          SectionCard(
            title: 'Status',
            child: _StatusGrid(status: status),
          ),
          const SizedBox(height: 16),
          SectionCard(
            title: 'Checkliste',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                _ChecklistLine(
                  done: status.connectionState == ConnectionStateKind.connected,
                  label: 'Verbunden',
                ),
                _ChecklistLine(
                  done: map.hasPerimeter,
                  label: 'Perimeter vorhanden',
                ),
                _ChecklistLine(
                  done: map.hasDock,
                  label: 'Dock gesetzt',
                ),
                _ChecklistLine(
                  done: mission != null,
                  label: 'Mission vorhanden',
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          SectionCard(
            title: 'Naechste Mission',
            child: mission == null
                ? const Text('Noch keine Mission vorhanden.')
                : _MissionCard(
                    missionName: mission.name,
                    scheduleLabel: mission.scheduleLabel ?? 'Manuell',
                    zoneNames: mission.zoneNames,
                    connected: isConnected,
                    busy: _commandBusy,
                    onStart: () => _runAction(() {
                      ref
                          .read(robotConnectionControllerProvider)
                          .startMowing(missionId: mission.id);
                    }),
                  ),
          ),
        ],
      ),
    );
  }

  Future<void> _runAction(VoidCallback action) async {
    if (_commandBusy) return;
    setState(() => _commandBusy = true);
    action();
    await Future<void>.delayed(const Duration(milliseconds: 350));
    if (mounted) setState(() => _commandBusy = false);
  }

  Future<void> _handleDisconnect() async {
    await ref.read(robotConnectionControllerProvider).disconnect();
    if (!mounted) return;
    context.go('/discover');
  }

  Future<void> _handleSwitchRobot() async {
    await ref.read(robotConnectionControllerProvider).switchRobot();
    if (!mounted) return;
    context.go('/discover');
  }
}

enum _DashboardMenuAction {
  switchRobot,
  disconnect,
}

class _StatusStrip extends StatelessWidget {
  const _StatusStrip({
    required this.status,
    required this.robotName,
  });

  final RobotStatus status;
  final String? robotName;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isConnected = status.connectionState == ConnectionStateKind.connected;
    final effectiveName = status.robotName ?? robotName ?? 'Kein Roboter';
    final details = <String>[
      effectiveName,
      isConnected ? 'verbunden' : 'nicht verbunden',
      status.statePhase ?? 'idle',
      status.rtkState ?? 'RTK unbekannt',
      status.batteryPercent != null ? 'Akku ${status.batteryPercent}%' : 'Akku --',
    ];

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: theme.cardColor,
        border: Border.all(color: theme.dividerColor),
        borderRadius: BorderRadius.circular(16),
      ),
      child: Row(
        children: <Widget>[
          Icon(
            Icons.circle,
            color: isConnected ? const Color(0xFF4ADE80) : const Color(0xFFF59E0B),
            size: 12,
          ),
          const SizedBox(width: 10),
          Expanded(child: Text(details.join(' · '))),
        ],
      ),
    );
  }
}

class _MiniMapCard extends StatelessWidget {
  const _MiniMapCard({
    required this.map,
    required this.status,
  });

  final MapGeometry map;
  final RobotStatus status;

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 1.7,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: const Color(0xFF08101D),
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: Theme.of(context).dividerColor),
        ),
        child: map.perimeter.length < 3
            ? const Center(
                child: Text('Noch keine Karte vorhanden'),
              )
            : ClipRRect(
                borderRadius: BorderRadius.circular(16),
                child: CustomPaint(
                  painter: _MiniMapPainter(
                    map: map,
                    robotX: status.x,
                    robotY: status.y,
                    robotHeading: status.heading,
                  ),
                  child: const SizedBox.expand(),
                ),
              ),
      ),
    );
  }
}

class _MiniMapPainter extends CustomPainter {
  const _MiniMapPainter({
    required this.map,
    required this.robotX,
    required this.robotY,
    required this.robotHeading,
  });

  final MapGeometry map;
  final double? robotX;
  final double? robotY;
  final double? robotHeading;

  @override
  void paint(Canvas canvas, Size size) {
    final allPoints = <MapPoint>[
      ...map.perimeter,
      ...map.dock,
      for (final polygon in map.noGoZones) ...polygon,
      for (final zone in map.zones) ...zone.points,
      if (robotX != null && robotY != null) MapPoint(x: robotX!, y: robotY!),
    ];
    if (allPoints.isEmpty) return;

    final minX = allPoints.map((point) => point.x).reduce((a, b) => a < b ? a : b);
    final maxX = allPoints.map((point) => point.x).reduce((a, b) => a > b ? a : b);
    final minY = allPoints.map((point) => point.y).reduce((a, b) => a < b ? a : b);
    final maxY = allPoints.map((point) => point.y).reduce((a, b) => a > b ? a : b);

    final spanX = (maxX - minX).abs() < 0.001 ? 1.0 : (maxX - minX);
    final spanY = (maxY - minY).abs() < 0.001 ? 1.0 : (maxY - minY);
    const padding = 18.0;
    final scale = ((size.width - padding * 2) / spanX)
        .clamp(0.0, double.infinity)
        .compareTo((size.height - padding * 2) / spanY) < 0
        ? (size.width - padding * 2) / spanX
        : (size.height - padding * 2) / spanY;

    Offset project(MapPoint point) {
      final dx = (point.x - minX) * scale + padding;
      final dy = size.height - ((point.y - minY) * scale + padding);
      return Offset(dx, dy);
    }

    final gridPaint = Paint()
      ..color = const Color(0xFF17304A)
      ..strokeWidth = 1;
    for (double x = 0; x < size.width; x += 28) {
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), gridPaint);
    }
    for (double y = 0; y < size.height; y += 28) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }

    _drawPolygon(
      canvas,
      map.perimeter,
      project,
      fillColor: const Color(0x1422C55E),
      strokeColor: const Color(0xFF22C55E),
      strokeWidth: 2.2,
    );

    for (final zone in map.zones) {
      _drawPolygon(
        canvas,
        zone.points,
        project,
        fillColor: const Color(0x102563EB),
        strokeColor: const Color(0xFF60A5FA),
        strokeWidth: 1.6,
      );
    }

    for (final exclusion in map.noGoZones) {
      _drawPolygon(
        canvas,
        exclusion,
        project,
        fillColor: const Color(0x18EF4444),
        strokeColor: const Color(0xFFF87171),
        strokeWidth: 1.4,
      );
    }

    if (map.dock.length >= 2) {
      final dockPath = Path()..moveTo(project(map.dock.first).dx, project(map.dock.first).dy);
      for (final point in map.dock.skip(1)) {
        final projected = project(point);
        dockPath.lineTo(projected.dx, projected.dy);
      }
      canvas.drawPath(
        dockPath,
        Paint()
          ..color = const Color(0xFFF59E0B)
          ..strokeWidth = 2
          ..style = PaintingStyle.stroke,
      );
    }

    if (robotX != null && robotY != null) {
      final robotCenter = project(MapPoint(x: robotX!, y: robotY!));
      final markerPaint = Paint()..color = const Color(0xFFF8FAFC);
      canvas.drawCircle(robotCenter, 6, markerPaint);
      canvas.drawCircle(
        robotCenter,
        10,
        Paint()
          ..color = const Color(0xFF38BDF8)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2,
      );

      final heading = robotHeading ?? 0;
      final tip = Offset(
        robotCenter.dx + 16 * MathHelper.cos(heading),
        robotCenter.dy - 16 * MathHelper.sin(heading),
      );
      canvas.drawLine(
        robotCenter,
        tip,
        Paint()
          ..color = const Color(0xFF38BDF8)
          ..strokeWidth = 2.4
          ..strokeCap = StrokeCap.round,
      );
    }
  }

  void _drawPolygon(
    Canvas canvas,
    List<MapPoint> points,
    Offset Function(MapPoint point) project, {
    required Color fillColor,
    required Color strokeColor,
    required double strokeWidth,
  }) {
    if (points.length < 3) return;
    final first = project(points.first);
    final path = Path()..moveTo(first.dx, first.dy);
    for (final point in points.skip(1)) {
      final projected = project(point);
      path.lineTo(projected.dx, projected.dy);
    }
    path.close();

    canvas.drawPath(
      path,
      Paint()
        ..color = fillColor
        ..style = PaintingStyle.fill,
    );
    canvas.drawPath(
      path,
      Paint()
        ..color = strokeColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = strokeWidth,
    );
  }

  @override
  bool shouldRepaint(covariant _MiniMapPainter oldDelegate) {
    return oldDelegate.map != map ||
        oldDelegate.robotX != robotX ||
        oldDelegate.robotY != robotY ||
        oldDelegate.robotHeading != robotHeading;
  }
}

class MathHelper {
  static double cos(double value) => math.cos(value);
  static double sin(double value) => math.sin(value);
}

class _QuickActions extends StatelessWidget {
  const _QuickActions({
    required this.connected,
    required this.busy,
    required this.onStart,
    required this.onStop,
    required this.onDock,
  });

  final bool connected;
  final bool busy;
  final VoidCallback? onStart;
  final VoidCallback onStop;
  final VoidCallback onDock;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: <Widget>[
        Expanded(
          child: FilledButton.icon(
            onPressed: connected && !busy ? onStart : null,
            icon: const Icon(Icons.play_arrow_rounded),
            label: const Text('Start'),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: OutlinedButton.icon(
            onPressed: connected && !busy ? onStop : null,
            icon: const Icon(Icons.pause_circle_rounded),
            label: const Text('Stop'),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: OutlinedButton.icon(
            onPressed: connected && !busy ? onDock : null,
            icon: const Icon(Icons.home_rounded),
            label: const Text('Dock'),
          ),
        ),
      ],
    );
  }
}

class _StatusGrid extends StatelessWidget {
  const _StatusGrid({
    required this.status,
  });

  final RobotStatus status;

  @override
  Widget build(BuildContext context) {
    return Wrap(
      spacing: 12,
      runSpacing: 12,
      children: <Widget>[
        _StatusChip(
          label: 'Phase',
          value: status.statePhase ?? 'idle',
        ),
        _StatusChip(
          label: 'GPS',
          value: status.rtkState ?? 'unbekannt',
        ),
        _StatusChip(
          label: 'Akku',
          value: status.batteryPercent != null ? '${status.batteryPercent}%' : '--',
        ),
        _StatusChip(
          label: 'Verbindung',
          value: switch (status.connectionState) {
            ConnectionStateKind.connected => 'online',
            ConnectionStateKind.connecting => 'verbinde',
            ConnectionStateKind.discovering => 'suche',
            ConnectionStateKind.error => 'fehler',
            ConnectionStateKind.disconnected => 'offline',
          },
        ),
      ],
    );
  }
}

class _StatusChip extends StatelessWidget {
  const _StatusChip({
    required this.label,
    required this.value,
  });

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      width: 150,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.28),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: theme.dividerColor),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(label, style: theme.textTheme.labelMedium),
          const SizedBox(height: 6),
          Text(value, style: theme.textTheme.titleMedium),
        ],
      ),
    );
  }
}

class _MissionCard extends StatelessWidget {
  const _MissionCard({
    required this.missionName,
    required this.scheduleLabel,
    required this.zoneNames,
    required this.connected,
    required this.busy,
    required this.onStart,
  });

  final String missionName;
  final String scheduleLabel;
  final List<String> zoneNames;
  final bool connected;
  final bool busy;
  final VoidCallback onStart;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(missionName, style: theme.textTheme.titleMedium),
        const SizedBox(height: 4),
        Text(scheduleLabel, style: theme.textTheme.bodySmall),
        const SizedBox(height: 12),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: zoneNames
              .map(
                (zone) => Chip(
                  label: Text(zone),
                  visualDensity: VisualDensity.compact,
                ),
              )
              .toList(),
        ),
        const SizedBox(height: 12),
        SizedBox(
          width: double.infinity,
          child: FilledButton.icon(
            onPressed: connected && !busy ? onStart : null,
            icon: const Icon(Icons.play_arrow_rounded),
            label: const Text('Mission starten'),
          ),
        ),
      ],
    );
  }
}

class _ChecklistLine extends StatelessWidget {
  const _ChecklistLine({
    required this.done,
    required this.label,
  });

  final bool done;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: <Widget>[
          Icon(
            done ? Icons.check_circle_rounded : Icons.radio_button_unchecked_rounded,
            size: 18,
            color: done ? const Color(0xFF4ADE80) : const Color(0xFF94A3B8),
          ),
          const SizedBox(width: 8),
          Text(label),
        ],
      ),
    );
  }
}
