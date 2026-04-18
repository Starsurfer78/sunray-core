import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../domain/map/map_geometry.dart';
import '../../../shared/widgets/robot_map_view.dart';

class MowSettingsScreen extends StatefulWidget {
  const MowSettingsScreen({super.key});

  @override
  State<MowSettingsScreen> createState() => _MowSettingsScreenState();
}

class _MowSettingsScreenState extends State<MowSettingsScreen> {
  bool _clearProgress = false;

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final zones = controller.mapGeometry.zones;
    final hasZones = zones.isNotEmpty;

    return Scaffold(
      backgroundColor: const Color(0xFFF2F2F6),
      body: SafeArea(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 8, 16, 0),
              child: Row(
                children: <Widget>[
                  IconButton(
                    onPressed: () {
                      if (context.canPop()) {
                        context.pop();
                      } else {
                        context.go('/dashboard');
                      }
                    },
                    icon: const Icon(Icons.arrow_back_rounded),
                  ),
                  const SizedBox(width: 4),
                  Expanded(
                    child: Text(
                      'Mäheinstellungen',
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(
                        color: const Color(0xFF1C1C1E),
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                ],
              ),
            ),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
                children: <Widget>[
                  ClipRRect(
                    borderRadius: BorderRadius.circular(24),
                    child: AspectRatio(
                      aspectRatio: 1.08,
                      child: RobotMapView(
                        map: controller.mapGeometry,
                        status: controller.connectionStatus,
                        interactive: false,
                        showPerimeter: true,
                        showZones: true,
                        showNoGo: true,
                        showDock: true,
                      ),
                    ),
                  ),
                  const SizedBox(height: 16),
                  if (hasZones)
                    Wrap(
                      spacing: 10,
                      runSpacing: 10,
                      children: <Widget>[
                        for (var i = 0; i < zones.length; i++)
                          _ZoneSummaryChip(
                            number: i + 1,
                            label: zones[i].name,
                            areaSqM: _polygonArea(zones[i].points),
                          ),
                      ],
                    )
                  else
                    const _NoZonesHint(),
                  const SizedBox(height: 20),
                  CheckboxListTile(
                    contentPadding: EdgeInsets.zero,
                    controlAffinity: ListTileControlAffinity.leading,
                    value: _clearProgress,
                    onChanged: (value) {
                      setState(() => _clearProgress = value ?? false);
                    },
                    title: const Text(
                      'Fortschritt löschen und von vorne beginnen',
                    ),
                  ),
                  const SizedBox(height: 18),
                  FilledButton(
                    onPressed: hasZones
                        ? () {
                            controller.startMowing(
                              clearProgressBeforeStart: _clearProgress,
                            );
                            context.go('/dashboard');
                          }
                        : null,
                    style: FilledButton.styleFrom(
                      backgroundColor: const Color(0xFF2F7D4A),
                      foregroundColor: Colors.white,
                      minimumSize: const Size.fromHeight(56),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(20),
                      ),
                    ),
                    child: const Text('Start'),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  int _polygonArea(List<MapPoint> points) {
    if (points.length < 3) return 0;
    var area = 0.0;
    for (var i = 0; i < points.length; i++) {
      final current = points[i];
      final next = points[(i + 1) % points.length];
      area += current.x * next.y - next.x * current.y;
    }
    return (area.abs() / 2).round();
  }
}

class _ZoneSummaryChip extends StatelessWidget {
  const _ZoneSummaryChip({
    required this.number,
    required this.label,
    required this.areaSqM,
  });

  final int number;
  final String label;
  final int areaSqM;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(999),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            CircleAvatar(
              radius: 12,
              backgroundColor: const Color(0xFF2F7D4A),
              child: Text(
                '$number',
                style: Theme.of(context).textTheme.labelSmall?.copyWith(
                  color: Colors.white,
                  fontWeight: FontWeight.w800,
                ),
              ),
            ),
            const SizedBox(width: 8),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                Text(
                  label,
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: const Color(0xFF1C1C1E),
                    fontWeight: FontWeight.w700,
                  ),
                ),
                if (areaSqM > 0)
                  Text(
                    '$areaSqM m²',
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFF6C6C70),
                    ),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _NoZonesHint extends StatelessWidget {
  const _NoZonesHint();

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xFFFFF3CD),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: const Color(0xFFE6A817)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Row(
          children: <Widget>[
            const Icon(
              Icons.warning_amber_rounded,
              color: Color(0xFFB07D11),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                'Keine Zonen definiert. Bitte zuerst Zonen in der Karte anlegen – der Roboter benötigt mindestens eine Zone zum Mähen.',
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: const Color(0xFF5A3E00),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
