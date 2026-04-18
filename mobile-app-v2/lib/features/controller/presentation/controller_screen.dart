import 'dart:async';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../shared/widgets/robot_map_view.dart';

class ControllerScreen extends StatefulWidget {
  const ControllerScreen({super.key});

  @override
  State<ControllerScreen> createState() => _ControllerScreenState();
}

class _ControllerScreenState extends State<ControllerScreen> {
  static const Duration _sendInterval = Duration(milliseconds: 80);

  AppController? _controller;
  double _linear = 0;
  double _angular = 0;
  double _driveLimit = 0.5;
  bool _mowMotorOn = false;
  Timer? _sendTimer;

  @override
  void initState() {
    super.initState();
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _controller = AppScope.read(context);
  }

  @override
  void dispose() {
    _sendTimer?.cancel();
    _controller?.sendManualDriveCommand(linear: 0, angular: 0);
    super.dispose();
  }

  void _updateDrive(
    AppController controller, {
    double? linear,
    double? angular,
  }) {
    if (linear != null) {
      _linear = linear;
    }
    if (angular != null) {
      _angular = angular;
    }
    controller.sendManualDriveCommand(
      linear: _linear * _driveLimit,
      angular: _angular * _driveLimit,
    );
    _syncDriveLoop(controller);
    setState(() {});
  }

  void _syncDriveLoop(AppController controller) {
    final active = _linear.abs() > 0.001 || _angular.abs() > 0.001;
    if (!active) {
      _sendTimer?.cancel();
      _sendTimer = null;
      return;
    }
    _sendTimer ??= Timer.periodic(_sendInterval, (_) {
      controller.sendManualDriveCommand(
        linear: _linear * _driveLimit,
        angular: _angular * _driveLimit,
      );
    });
  }

  Future<bool> _confirmMowMotorActivation() async {
    final confirmed = await showDialog<bool>(
      context: context,
      barrierDismissible: false,
      builder: (dialogContext) {
        return AlertDialog(
          title: const Text('Mähmotor einschalten?'),
          content: const Text(
            'Der Mähmotor startet sofort. Stelle sicher, dass der Bereich frei ist und sich keine Personen, Tiere oder Gegenstände im Gefahrenbereich befinden.',
          ),
          actions: <Widget>[
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(false),
              child: const Text('Abbrechen'),
            ),
            FilledButton(
              onPressed: () => Navigator.of(dialogContext).pop(true),
              child: const Text('Warnung bestätigen'),
            ),
          ],
        );
      },
    );
    return confirmed ?? false;
  }

  Future<void> _toggleMowMotor(AppController controller) async {
    final nextState = !_mowMotorOn;
    final messenger = ScaffoldMessenger.of(context);
    if (nextState) {
      final confirmed = await _confirmMowMotorActivation();
      if (!mounted || !confirmed) {
        return;
      }
    }
    final result = await controller.setManualMowMotor(nextState);
    if (!mounted) {
      return;
    }
    if (result != null) {
      messenger.showSnackBar(SnackBar(content: Text(result)));
      return;
    }
    setState(() => _mowMotorOn = nextState);
    messenger.showSnackBar(
      SnackBar(
        content: Text(
          nextState ? 'Mähmotor eingeschaltet.' : 'Mähmotor ausgeschaltet.',
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final isConnected = controller.connectionStatus.connectionState ==
        ConnectionStateKind.connected;
    final status = isConnected ? controller.connectionStatus : null;
    final inIdle = isConnected &&
        (controller.connectionStatus.statePhase == null ||
            controller.connectionStatus.statePhase == 'idle');

    return Scaffold(
      backgroundColor: const Color(0xFF08111F),
      body: Stack(
        children: <Widget>[
          Positioned.fill(
            child: RobotMapView(
              map: controller.mapGeometry,
              status: status,
              interactive: false,
              showCenterButton: false,
            ),
          ),
          Positioned.fill(
            child: IgnorePointer(
              child: DecoratedBox(
                decoration: const BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                    colors: <Color>[
                      Color(0xAA020617),
                      Color(0x22020617),
                      Color(0xCC020617),
                    ],
                  ),
                ),
              ),
            ),
          ),
          SafeArea(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 16),
              child: Column(
                children: <Widget>[
                  Row(
                    children: <Widget>[
                      _ControllerIconButton(
                        icon: Icons.arrow_back_rounded,
                        onTap: () {
                          controller.sendManualDriveCommand(
                            linear: 0,
                            angular: 0,
                          );
                          context.pop();
                        },
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: _InfoCard(
                          child: Row(
                            children: <Widget>[
                              const Icon(
                                Icons.sports_esports_rounded,
                                color: Colors.white,
                              ),
                              const SizedBox(width: 10),
                              Expanded(
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  mainAxisSize: MainAxisSize.min,
                                  children: <Widget>[
                                    Text(
                                      'Controller-Modus',
                                      style: Theme.of(context)
                                          .textTheme
                                          .titleMedium
                                          ?.copyWith(
                                            color: Colors.white,
                                            fontWeight: FontWeight.w800,
                                          ),
                                    ),
                                    Text(
                                      !isConnected
                                          ? 'Nicht verbunden — bitte zuerst Alfred verbinden.'
                                          : inIdle
                                          ? 'Idle erkannt — Sticks sind aktiv.'
                                          : 'Nicht in Idle — Kommandos werden vom Backend ignoriert.',
                                      style: Theme.of(context)
                                          .textTheme
                                          .bodySmall
                                          ?.copyWith(
                                            color: const Color(0xFFCBD5E1),
                                          ),
                                    ),
                                  ],
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                      const SizedBox(width: 12),
                      _ControllerIconButton(
                        icon: Icons.stop_circle_outlined,
                        color: const Color(0xFFDC2626),
                        onTap: () {
                          controller.emergencyStopRobot();
                          _updateDrive(controller, linear: 0, angular: 0);
                        },
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: <Widget>[
                        Expanded(
                          flex: 3,
                          child: Align(
                            alignment: Alignment.bottomLeft,
                            child: _DroneStick(
                              title: 'Lenkung',
                              accentColor: const Color(0xFF38BDF8),
                              onChanged: (offset) => _updateDrive(
                                controller,
                                angular: -offset.dx,
                              ),
                            ),
                          ),
                        ),
                        Expanded(
                          flex: 2,
                          child: _ControlPanel(
                            driveLimit: _driveLimit,
                            mowMotorOn: _mowMotorOn,
                            linear: _linear * _driveLimit,
                            angular: _angular * _driveLimit,
                            connectionStatus: controller.connectionStatus,
                            onDriveLimitChanged: (value) {
                              setState(() => _driveLimit = value);
                              controller.sendManualDriveCommand(
                                linear: _linear * _driveLimit,
                                angular: _angular * _driveLimit,
                              );
                            },
                            onToggleMowMotor: () =>
                                _toggleMowMotor(controller),
                          ),
                        ),
                        Expanded(
                          flex: 3,
                          child: Align(
                            alignment: Alignment.bottomRight,
                            child: _DroneStick(
                              title: 'Fahrt',
                              accentColor: const Color(0xFF22C55E),
                              onChanged: (offset) =>
                                  _updateDrive(controller, linear: offset.dy),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _DroneStick extends StatefulWidget {
  const _DroneStick({
    required this.title,
    required this.accentColor,
    required this.onChanged,
  });

  final String title;
  final Color accentColor;
  final ValueChanged<Offset> onChanged;

  @override
  State<_DroneStick> createState() => _DroneStickState();
}

class _DroneStickState extends State<_DroneStick> {
  static const double _radius = 74;
  static const double _thumbRadius = 24;
  Offset _offset = Offset.zero;

  void _update(Offset localPosition) {
    const center = Offset(_radius, _radius);
    var delta = localPosition - center;
    final distance = delta.distance;
    if (distance > _radius) {
      delta = delta / distance * _radius;
    }
    setState(() => _offset = delta);
    widget.onChanged(
      Offset(
        (delta.dx / _radius).clamp(-1.0, 1.0),
        (-delta.dy / _radius).clamp(-1.0, 1.0),
      ),
    );
  }

  void _reset() {
    setState(() => _offset = Offset.zero);
    widget.onChanged(Offset.zero);
  }

  @override
  Widget build(BuildContext context) {
    const diameter = _radius * 2;
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        Text(
          widget.title,
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
            color: Colors.white,
            fontWeight: FontWeight.w800,
          ),
        ),
        const SizedBox(height: 12),
        GestureDetector(
          onPanStart: (details) => _update(details.localPosition),
          onPanUpdate: (details) => _update(details.localPosition),
          onPanEnd: (_) => _reset(),
          onPanCancel: _reset,
          child: SizedBox(
            width: diameter,
            height: diameter,
            child: CustomPaint(
              painter: _DroneStickPainter(
                offset: _offset,
                accentColor: widget.accentColor,
                radius: _radius,
                thumbRadius: _thumbRadius,
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _DroneStickPainter extends CustomPainter {
  const _DroneStickPainter({
    required this.offset,
    required this.accentColor,
    required this.radius,
    required this.thumbRadius,
  });

  final Offset offset;
  final Color accentColor;
  final double radius;
  final double thumbRadius;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final ringPaint = Paint()
      ..color = const Color(0xAA0F172A)
      ..style = PaintingStyle.fill;
    final borderPaint = Paint()
      ..color = const Color(0x4493C5FD)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;
    final guidePaint = Paint()
      ..color = const Color(0x33E2E8F0)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.5;
    final thumbPaint = Paint()
      ..shader =
          RadialGradient(
            colors: <Color>[
              accentColor.withValues(alpha: 0.95),
              accentColor.withValues(alpha: 0.55),
            ],
          ).createShader(
            Rect.fromCircle(center: center + offset, radius: thumbRadius),
          );

    canvas.drawCircle(center, radius, ringPaint);
    canvas.drawCircle(center, radius, borderPaint);
    canvas.drawLine(
      Offset(center.dx - radius, center.dy),
      Offset(center.dx + radius, center.dy),
      guidePaint,
    );
    canvas.drawLine(
      Offset(center.dx, center.dy - radius),
      Offset(center.dx, center.dy + radius),
      guidePaint,
    );
    canvas.drawCircle(center + offset, thumbRadius, thumbPaint);
  }

  @override
  bool shouldRepaint(covariant _DroneStickPainter oldDelegate) {
    return oldDelegate.offset != offset ||
        oldDelegate.accentColor != accentColor;
  }
}

class _ControlPanel extends StatelessWidget {
  const _ControlPanel({
    required this.driveLimit,
    required this.mowMotorOn,
    required this.linear,
    required this.angular,
    required this.connectionStatus,
    required this.onDriveLimitChanged,
    required this.onToggleMowMotor,
  });

  final double driveLimit;
  final bool mowMotorOn;
  final double linear;
  final double angular;
  final RobotStatus connectionStatus;
  final ValueChanged<double> onDriveLimitChanged;
  final VoidCallback onToggleMowMotor;

  @override
  Widget build(BuildContext context) {
    final connected =
        connectionStatus.connectionState == ConnectionStateKind.connected;

    return _InfoCard(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            'Antriebslimit ${(driveLimit * 100).round()}%',
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
              color: Colors.white,
              fontWeight: FontWeight.w700,
            ),
          ),
          Slider(
            value: driveLimit,
            min: 0.2,
            max: 1.0,
            divisions: 8,
            label: '${(driveLimit * 100).round()}%',
            onChanged: onDriveLimitChanged,
          ),
          Row(
            children: <Widget>[
              Expanded(
                child: _TelemetryTile(
                  label: 'Linear',
                  value: linear.toStringAsFixed(2),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: _TelemetryTile(
                  label: 'Angular',
                  value: angular.toStringAsFixed(2),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: FilledButton.icon(
              onPressed: connected ? onToggleMowMotor : null,
              style: FilledButton.styleFrom(
                backgroundColor: mowMotorOn
                    ? const Color(0xFFDC2626)
                    : const Color(0xFF16A34A),
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 12),
              ),
              icon: Icon(
                mowMotorOn ? Icons.power_settings_new : Icons.grass_rounded,
              ),
              label: Text(
                mowMotorOn ? 'Mähmotor aus' : 'Mähmotor ein',
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _TelemetryTile extends StatelessWidget {
  const _TelemetryTile({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: const Color(0x66020617),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: const Color(0x331E3A5F)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            label,
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: const Color(0xFF94A3B8)),
          ),
          const SizedBox(height: 2),
          Text(
            value,
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              color: Colors.white,
              fontWeight: FontWeight.w800,
            ),
          ),
        ],
      ),
    );
  }
}

class _ControllerIconButton extends StatelessWidget {
  const _ControllerIconButton({
    required this.icon,
    required this.onTap,
    this.color = const Color(0xFF0F172A),
  });

  final IconData icon;
  final VoidCallback onTap;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: color,
      borderRadius: BorderRadius.circular(18),
      child: InkWell(
        borderRadius: BorderRadius.circular(18),
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(14),
          child: Icon(icon, color: Colors.white),
        ),
      ),
    );
  }
}

class _InfoCard extends StatelessWidget {
  const _InfoCard({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xCC0F172A),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: const Color(0x331E3A5F)),
      ),
      child: Padding(padding: const EdgeInsets.all(16), child: child),
    );
  }
}
