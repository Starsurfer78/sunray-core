import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../shared/widgets/robot_map_view.dart';

class MissionsScreen extends StatelessWidget {
  const MissionsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final missions = controller.missions;

    return Scaffold(
      backgroundColor: const Color(0xFFF5F7F2),
      body: SafeArea(
        child: Column(
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 8, 16, 12),
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
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        Text(
                          'Missionen',
                          style: Theme.of(context).textTheme.titleLarge
                              ?.copyWith(fontWeight: FontWeight.w800),
                        ),
                        Text(
                          'Planung, Bedingungen und Startfreigabe',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                  FilledButton.icon(
                    onPressed: () {
                      final mission = AppScope.read(context).createMission();
                      context.push('/missions/detail/${mission.id}');
                    },
                    style: FilledButton.styleFrom(
                      backgroundColor: const Color(0xFF2F7D4A),
                      foregroundColor: Colors.white,
                    ),
                    icon: const Icon(Icons.add_rounded),
                    label: const Text('Mission anlegen'),
                  ),
                ],
              ),
            ),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: _MissionMapPreview(controller: controller),
            ),
            const SizedBox(height: 12),
            Expanded(
              child: missions.isEmpty
                  ? const _EmptyMissionState()
                  : ListView.builder(
                      padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
                      itemCount: missions.length,
                      itemBuilder: (context, index) {
                        final mission = missions[index];
                        return Padding(
                          padding: const EdgeInsets.only(bottom: 12),
                          child: _MissionListCard(
                            mission: mission,
                            onTap: () =>
                                context.push('/missions/detail/${mission.id}'),
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

class _MissionMapPreview extends StatelessWidget {
  const _MissionMapPreview({required this.controller});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            ClipRRect(
              borderRadius: BorderRadius.circular(22),
              child: AspectRatio(
                aspectRatio: 1.48,
                child: controller.hasMap
                    ? RobotMapView(
                        map: controller.mapGeometry,
                        status: controller.connectionStatus,
                        interactive: false,
                        showCenterButton: false,
                      )
                    : const ColoredBox(color: Color(0xFFEFF2ED)),
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: <Widget>[
                _LegendChip(
                  color: const Color(0xFF2F7D4A),
                  label: controller.hasMap
                      ? '${controller.mapAreaSquareMeters} m² Kartenfläche'
                      : 'Noch keine Karte',
                ),
                _LegendChip(
                  color: const Color(0xFFC6463B),
                  label: '${controller.noGoCount} No-Go',
                ),
                _LegendChip(
                  color: const Color(0xFFF59E0B),
                  label: controller.hasZones
                      ? '${controller.zoneCount} Zonen verfügbar'
                      : 'Gesamte Fläche möglich',
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _EmptyMissionState extends StatelessWidget {
  const _EmptyMissionState();

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 32),
        child: Text(
          'Noch keine Mission angelegt. Du kannst direkt eine Mission für die ganze Karte oder später gezielt für einzelne Zonen erstellen.',
          textAlign: TextAlign.center,
          style: Theme.of(
            context,
          ).textTheme.bodyMedium?.copyWith(color: const Color(0xFF667267)),
        ),
      ),
    );
  }
}

class _MissionListCard extends StatelessWidget {
  const _MissionListCard({required this.mission, required this.onTap});

  final AppMission mission;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final statusColor = switch (mission.statusLabel) {
      'Aktiv' => const Color(0xFF2F7D4A),
      'Pausiert' => const Color(0xFF8A8A90),
      'Entwurf' => const Color(0xFF2F7D4A),
      _ => const Color(0xFF2F7D4A),
    };

    return Material(
      color: Colors.white,
      borderRadius: BorderRadius.circular(24),
      child: InkWell(
        borderRadius: BorderRadius.circular(24),
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(18),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Row(
                children: <Widget>[
                  Expanded(
                    child: Text(
                      mission.name,
                      style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 10,
                      vertical: 6,
                    ),
                    decoration: BoxDecoration(
                      color: statusColor.withValues(alpha: 0.12),
                      borderRadius: BorderRadius.circular(999),
                    ),
                    child: Text(
                      mission.statusLabel,
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: statusColor,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: mission.zones.isEmpty
                    ? const <Widget>[
                        _MissionPill(label: 'Gesamte Karte', accent: false),
                      ]
                    : mission.zones
                          .map(
                            (zone) => _MissionPill(label: zone, accent: true),
                          )
                          .toList(),
              ),
              const SizedBox(height: 12),
              Row(
                children: <Widget>[
                  Expanded(
                    child: _MissionMetaBlock(
                      label: 'Zeitplan',
                      value: mission.scheduleLabel,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _MissionMetaBlock(
                      label: 'Bedingungen',
                      value: mission.onlyWhenDry
                          ? 'Trocken + Akku'
                          : mission.requiresHighBattery
                          ? 'Akku zuerst'
                          : 'Flexibel',
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: <Widget>[
                  Icon(
                    mission.optimizeOrder
                        ? Icons.route_rounded
                        : Icons.swap_vert_rounded,
                    size: 18,
                    color: const Color(0xFF667267),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      mission.optimizeOrder
                          ? 'Reihenfolge optimiert'
                          : 'Reihenfolge manuell festgelegt',
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: const Color(0xFF667267),
                      ),
                    ),
                  ),
                  const Icon(
                    Icons.chevron_right_rounded,
                    color: Color(0xFF667267),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _LegendChip extends StatelessWidget {
  const _LegendChip({required this.color, required this.label});

  final Color color;
  final String label;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(999),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Container(
              width: 10,
              height: 10,
              decoration: BoxDecoration(color: color, shape: BoxShape.circle),
            ),
            const SizedBox(width: 8),
            Text(
              label,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: const Color(0xFF1F2A22),
                fontWeight: FontWeight.w700,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _MissionPill extends StatelessWidget {
  const _MissionPill({required this.label, required this.accent});

  final String label;
  final bool accent;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: accent ? const Color(0x142F7D4A) : const Color(0xFFEFF2ED),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        child: Text(
          label,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            color: accent ? const Color(0xFF2F7D4A) : const Color(0xFF667267),
            fontWeight: FontWeight.w700,
          ),
        ),
      ),
    );
  }
}

class _MissionMetaBlock extends StatelessWidget {
  const _MissionMetaBlock({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(
          label,
          style: Theme.of(
            context,
          ).textTheme.bodySmall?.copyWith(color: const Color(0xFF667267)),
        ),
        const SizedBox(height: 4),
        Text(
          value,
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
            color: const Color(0xFF1F2A22),
            fontWeight: FontWeight.w700,
          ),
        ),
      ],
    );
  }
}
