import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

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

  @override
  Widget build(BuildContext context) {
    final missions = ref.watch(missionsProvider);
    final mission = missions.isEmpty ? null : missions.first;
    final map = ref.watch(mapGeometryProvider);
    final status = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final isConnected = status.connectionState == ConnectionStateKind.connected;

    final hasConnectionNotice = status.lastError != null &&
        (status.connectionState == ConnectionStateKind.error ||
            status.connectionState == ConnectionStateKind.connecting);

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
      body: Column(
        children: <Widget>[
          // Map area (takes most of the screen)
          Expanded(
            flex: 3,
            child: Stack(
              children: <Widget>[
                // The map itself
                Positioned.fill(
                  child: RobotMapView(
                    map: map,
                    status: status,
                    interactive: true,
                  ),
                ),
                // Status overlay (top-left)
                Positioned(
                  top: 10,
                  left: 10,
                  child: _StatusOverlay(status: status),
                ),
                // Joystick FAB (bottom-right)
                Positioned(
                  bottom: 14,
                  right: 14,
                  child: FloatingActionButton(
                    heroTag: 'joystick_fab',
                    tooltip: 'Virtuelles Steuerkreuz',
                    onPressed: isConnected ? _openJoystick : null,
                    backgroundColor: isConnected
                        ? const Color(0xFF1E40AF)
                        : const Color(0xFF334155),
                    child: const Icon(Icons.gamepad_rounded),
                  ),
                ),
              ],
            ),
          ),
          // Connection notice (conditionally shown between map and actions)
          if (hasConnectionNotice)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              child: ConnectionNotice(status: status),
            ),
          // Bottom action bar
          _BottomActionBar(
            isConnected: isConnected,
            busy: _commandBusy,
            missionName: mission?.name,
            onMow: mission == null
                ? null
                : () => _runAction(() {
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
          // Joystick overlay
          if (_joystickVisible)
            _JoystickOverlay(
              onDrive: (vx, vy) {
                // vy = forward/backward (linear), -vx = left/right rotation (angular, negative = right)
                ref.read(robotConnectionControllerProvider).sendDriveCommand(vy, -vx);
              },
              onClose: () {
                ref.read(robotConnectionControllerProvider).sendDriveCommand(0, 0);
                setState(() => _joystickVisible = false);
              },
            ),
        ],
      ),
    );
  }

  void _openJoystick() {
    setState(() => _joystickVisible = true);
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

// ---------------------------------------------------------------------------
// Status overlay chips on the map
// ---------------------------------------------------------------------------

class _StatusOverlay extends StatelessWidget {
  const _StatusOverlay({required this.status});

  final RobotStatus status;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        _OverlayChip(
          icon: Icons.battery_charging_full_rounded,
          label: status.batteryPercent != null
              ? '${status.batteryPercent}%'
              : '--',
        ),
        const SizedBox(height: 4),
        _OverlayChip(
          icon: Icons.gps_fixed_rounded,
          label: status.rtkState ?? 'GPS --',
        ),
        const SizedBox(height: 4),
        _OverlayChip(
          icon: Icons.directions_walk_rounded,
          label: status.statePhase ?? 'idle',
        ),
      ],
    );
  }
}

class _OverlayChip extends StatelessWidget {
  const _OverlayChip({
    required this.icon,
    required this.label,
  });

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
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
          Icon(icon, size: 13, color: const Color(0xFFCBD5E1)),
          const SizedBox(width: 5),
          Text(
            label,
            style: const TextStyle(
              color: Color(0xFFE2E8F0),
              fontSize: 12,
              fontWeight: FontWeight.w500,
            ),
          ),
        ],
      ),
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
    required this.onMow,
    required this.onStop,
    required this.onDock,
  });

  final bool isConnected;
  final bool busy;
  final String? missionName;
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
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Row(
              children: <Widget>[
                Expanded(
                  child: FilledButton.icon(
                    onPressed: isConnected && !busy ? onMow : null,
                    icon: const Icon(Icons.play_arrow_rounded, size: 18),
                    label: const Text('Maehen'),
                    style: FilledButton.styleFrom(
                      padding: const EdgeInsets.symmetric(vertical: 10),
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: isConnected && !busy ? onStop : null,
                    icon: const Icon(Icons.pause_circle_rounded, size: 18),
                    label: const Text('Stop'),
                    style: OutlinedButton.styleFrom(
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
                      padding: const EdgeInsets.symmetric(vertical: 10),
                    ),
                  ),
                ),
              ],
            ),
            if (missionName != null) ...<Widget>[
              const SizedBox(height: 8),
              Row(
                children: <Widget>[
                  const Icon(Icons.route_rounded, size: 14, color: Color(0xFF94A3B8)),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      missionName!,
                      style: theme.textTheme.bodySmall,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                ],
              ),
            ],
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
  const _JoystickOverlay({
    required this.onDrive,
    required this.onClose,
  });

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

  void _onPanStart(DragStartDetails details) {
    _updateThumb(details.localPosition);
    _sendTimer ??= Timer.periodic(_sendInterval, (_) => _emitCurrent());
  }

  void _onPanUpdate(DragUpdateDetails details) {
    _updateThumb(details.localPosition);
  }

  void _onPanEnd(DragEndDetails details) {
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

  void _updateThumb(Offset localPosition) {
    const center = Offset(_joystickRadius, _joystickRadius);
    final delta = localPosition - center;
    final distance = delta.distance;
    final clamped = distance <= _joystickRadius
        ? delta
        : delta / distance * _joystickRadius;
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
                  const Text(
                    'Steuerkreuz',
                    style: TextStyle(
                      color: Color(0xFFCBD5E1),
                      fontSize: 14,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
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
                    icon: const Icon(Icons.close_rounded, color: Color(0xFF94A3B8)),
                    label: const Text(
                      'Schliessen',
                      style: TextStyle(color: Color(0xFF94A3B8)),
                    ),
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
  const _JoystickPainter({
    required this.thumbOffset,
    required this.joystickRadius,
    required this.thumbRadius,
  });

  final Offset thumbOffset;
  final double joystickRadius;
  final double thumbRadius;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);

    // Outer ring
    canvas.drawCircle(
      center,
      joystickRadius,
      Paint()
        ..color = const Color(0x441E40AF)
        ..style = PaintingStyle.fill,
    );
    canvas.drawCircle(
      center,
      joystickRadius,
      Paint()
        ..color = const Color(0xFF3B82F6)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );

    // Center dot
    canvas.drawCircle(
      center,
      6,
      Paint()..color = const Color(0x664ADE80),
    );

    // Crosshair lines
    final linePaint = Paint()
      ..color = const Color(0x443B82F6)
      ..strokeWidth = 1;
    canvas.drawLine(
      Offset(center.dx - joystickRadius, center.dy),
      Offset(center.dx + joystickRadius, center.dy),
      linePaint,
    );
    canvas.drawLine(
      Offset(center.dx, center.dy - joystickRadius),
      Offset(center.dx, center.dy + joystickRadius),
      linePaint,
    );

    // Thumb
    final thumbCenter = center + thumbOffset;
    canvas.drawCircle(
      thumbCenter,
      thumbRadius,
      Paint()..color = const Color(0xFF2563EB),
    );
    canvas.drawCircle(
      thumbCenter,
      thumbRadius,
      Paint()
        ..color = const Color(0xFF93C5FD)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );
    // Inner highlight dot
    canvas.drawCircle(
      thumbCenter,
      thumbRadius * 0.35,
      Paint()..color = const Color(0xAABAE6FD),
    );
  }

  @override
  bool shouldRepaint(covariant _JoystickPainter oldDelegate) {
    return oldDelegate.thumbOffset != thumbOffset;
  }
}

// Cross-platform math helper kept for any future use
class MathHelper {
  static double cos(double value) => math.cos(value);
  static double sin(double value) => math.sin(value);
}
