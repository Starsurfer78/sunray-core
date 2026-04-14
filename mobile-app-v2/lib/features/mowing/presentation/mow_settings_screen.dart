import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../shared/widgets/fake_map_canvas.dart';

class MowSettingsScreen extends StatefulWidget {
  const MowSettingsScreen({super.key});

  @override
  State<MowSettingsScreen> createState() => _MowSettingsScreenState();
}

class _MowSettingsScreenState extends State<MowSettingsScreen> {
  bool _reorderZones = true;
  bool _clearProgress = false;

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final zones = _buildZones(controller);

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
                  _SettingToggleCard(
                    title: 'Mähablauf anpassen',
                    subtitle: _reorderZones
                        ? 'Alfred priorisiert die empfohlene Reihenfolge für diesen Lauf.'
                        : 'Alfred mäht die Bereiche in der aktuellen Reihenfolge.',
                    value: _reorderZones,
                    onChanged: (value) {
                      setState(() => _reorderZones = value);
                    },
                  ),
                  const SizedBox(height: 16),
                  ClipRRect(
                    borderRadius: BorderRadius.circular(24),
                    child: AspectRatio(
                      aspectRatio: 1.08,
                      child: Stack(
                        children: <Widget>[
                          Positioned.fill(
                            child: FakeMapCanvas(
                              borderRadius: 0,
                              showToolbar: false,
                              showNoGoLabel: controller.noGoCount > 0,
                              showDockLabel: controller.hasMap,
                              zones: zones,
                              selectedZoneId: null,
                            ),
                          ),
                          ...zones.map(
                            (zone) => Positioned(
                              left: zone.left,
                              top: zone.top + 36,
                              child: _ZoneOrderBadge(
                                number: _badgeNumberForZone(zone.id),
                                enabled: controller.hasZones,
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 16),
                  if (controller.hasZones)
                    Wrap(
                      spacing: 10,
                      runSpacing: 10,
                      children: <Widget>[
                        _ZoneSummaryChip(
                          number: 1,
                          label: _reorderZones
                              ? 'Hinterer Garten'
                              : 'Seitengarten',
                        ),
                        _ZoneSummaryChip(
                          number: 2,
                          label: _reorderZones
                              ? 'Seitengarten'
                              : 'Hinterer Garten',
                        ),
                      ],
                    )
                  else
                    const _WholeMapHint(),
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
                    onPressed: () {
                      controller.startMowing(
                        reorderZones: _reorderZones,
                        clearProgressBeforeStart: _clearProgress,
                      );
                      context.go('/dashboard');
                    },
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

  List<FakeMapZoneOverlay> _buildZones(AppController controller) {
    if (!controller.hasZones) {
      return const <FakeMapZoneOverlay>[
        FakeMapZoneOverlay(
          id: 'whole-map',
          label: 'Gesamte Karte',
          coverageLabel: '210/5000 m²',
          lastRunLabel: 'Bereit für den ersten Lauf',
          left: 92,
          top: 48,
          popupLeft: 48,
          popupTop: 96,
        ),
      ];
    }

    final ordered = _reorderZones
        ? const <FakeMapZoneOverlay>[
            FakeMapZoneOverlay(
              id: 'zone-a',
              label: 'Hinterer Garten',
              coverageLabel: '253/512 m²',
              lastRunLabel: 'Priorität 1',
              left: 18,
              top: 18,
              popupLeft: 18,
              popupTop: 58,
            ),
            FakeMapZoneOverlay(
              id: 'zone-b',
              label: 'Seitengarten',
              coverageLabel: '184/386 m²',
              lastRunLabel: 'Priorität 2',
              left: 188,
              top: 116,
              popupLeft: 116,
              popupTop: 156,
            ),
          ]
        : const <FakeMapZoneOverlay>[
            FakeMapZoneOverlay(
              id: 'zone-b',
              label: 'Seitengarten',
              coverageLabel: '184/386 m²',
              lastRunLabel: 'Priorität 1',
              left: 188,
              top: 116,
              popupLeft: 116,
              popupTop: 156,
            ),
            FakeMapZoneOverlay(
              id: 'zone-a',
              label: 'Hinterer Garten',
              coverageLabel: '253/512 m²',
              lastRunLabel: 'Priorität 2',
              left: 18,
              top: 18,
              popupLeft: 18,
              popupTop: 58,
            ),
          ];

    return ordered.take(controller.zoneCount.clamp(0, ordered.length)).toList();
  }

  int _badgeNumberForZone(String zoneId) {
    if (!_reorderZones && zoneId == 'zone-b') {
      return 1;
    }
    if (!_reorderZones && zoneId == 'zone-a') {
      return 2;
    }
    if (zoneId == 'zone-b') {
      return 2;
    }
    return 1;
  }
}

class _SettingToggleCard extends StatelessWidget {
  const _SettingToggleCard({
    required this.title,
    required this.subtitle,
    required this.value,
    required this.onChanged,
  });

  final String title;
  final String subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(20),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 14, 16, 14),
        child: Row(
          children: <Widget>[
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    title,
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      color: const Color(0xFF1C1C1E),
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    subtitle,
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      color: const Color(0xFF6C6C70),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 12),
            Switch.adaptive(value: value, onChanged: onChanged),
          ],
        ),
      ),
    );
  }
}

class _ZoneOrderBadge extends StatelessWidget {
  const _ZoneOrderBadge({required this.number, required this.enabled});

  final int number;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 28,
      height: 28,
      decoration: BoxDecoration(
        color: enabled ? const Color(0xFF2F7D4A) : const Color(0xFF8A8A90),
        shape: BoxShape.circle,
        border: Border.all(color: Colors.white, width: 2),
      ),
      alignment: Alignment.center,
      child: Text(
        '$number',
        style: Theme.of(context).textTheme.labelLarge?.copyWith(
          color: Colors.white,
          fontWeight: FontWeight.w800,
        ),
      ),
    );
  }
}

class _ZoneSummaryChip extends StatelessWidget {
  const _ZoneSummaryChip({required this.number, required this.label});

  final int number;
  final String label;

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
            Text(
              label,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: const Color(0xFF1C1C1E),
                fontWeight: FontWeight.w700,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _WholeMapHint extends StatelessWidget {
  const _WholeMapHint();

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(18),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Row(
          children: <Widget>[
            const Icon(Icons.map_rounded, color: Color(0xFF2F7D4A)),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                'Ohne Zonen startet Alfred einen kompletten Lauf für die ganze Karte.',
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: const Color(0xFF3A3A3C),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
