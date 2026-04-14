import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../domain/map_editor_state.dart';

class AddMapObjectScreen extends StatelessWidget {
  const AddMapObjectScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final bottomInset = MediaQuery.paddingOf(context).bottom;
    final canCreateZone = controller.mapGeometry.perimeter.length >= 3 &&
        controller.mapGeometry.dock.isNotEmpty;

    return Scaffold(
      backgroundColor: const Color(0xFFE8E8EC),
      body: SafeArea(
        child: Stack(
          children: <Widget>[
            Positioned.fill(
              child: DecoratedBox(
                decoration: const BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                    colors: <Color>[Color(0xFFF2F2F6), Color(0xFFE8E8EC)],
                  ),
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 120),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: <Widget>[
                            Text(
                              'Objekt hinzufügen',
                              style: Theme.of(context).textTheme.headlineSmall
                                  ?.copyWith(
                                    color: const Color(0xFF1C1C1E),
                                    fontWeight: FontWeight.w800,
                                  ),
                            ),
                            const SizedBox(height: 6),
                            Text(
                              'Wähle aus, welches Kartenobjekt du als Nächstes '
                              'auf Alfreds Karte anlegen willst.',
                              style: Theme.of(context).textTheme.bodyMedium
                                  ?.copyWith(color: const Color(0xFF6C6C70)),
                            ),
                          ],
                        ),
                      ),
                      const SizedBox(width: 12),
                      _RoundCloseButton(
                        onTap: () {
                          if (context.canPop()) {
                            context.pop();
                          } else {
                            context.go('/map');
                          }
                        },
                      ),
                    ],
                  ),
                  const SizedBox(height: 28),
                  Expanded(
                    child: GridView.count(
                      crossAxisCount: 2,
                      mainAxisSpacing: 14,
                      crossAxisSpacing: 14,
                      childAspectRatio: 0.92,
                      children: <Widget>[
                        _ObjectTile(
                          icon: Icons.crop_square_rounded,
                          title: 'Perimeter',
                          subtitle: controller.hasMap
                              ? 'Grenze bearbeiten'
                              : 'Erste Begrenzung',
                          detail: controller.mapGeometry.perimeter.isNotEmpty
                              ? '${controller.perimeterPointCount} Punkte'
                              : 'Karte anlegen',
                          onTap: () {
                            controller.createMapObject(
                              EditableMapObjectType.perimeter,
                            );
                            context.pop();
                          },
                        ),
                        _ObjectTile(
                          icon: Icons.block_rounded,
                          title: 'No-Go',
                          subtitle: 'Sperrzone setzen',
                          detail: '${controller.noGoCount} Bereiche',
                          onTap: () {
                            controller.createMapObject(
                              EditableMapObjectType.noGo,
                            );
                            context.pop();
                          },
                        ),
                        _ObjectTile(
                          icon: Icons.home_work_rounded,
                          title: 'Dock',
                          subtitle: 'Station setzen',
                          detail: controller.mapGeometry.dock.isNotEmpty
                              ? '${controller.mapGeometry.dock.length} Punkte'
                              : 'Noch nicht gesetzt',
                          onTap: () {
                            controller.createMapObject(
                              EditableMapObjectType.dock,
                            );
                            context.pop();
                          },
                        ),
                        _ObjectTile(
                          icon: Icons.grass_rounded,
                          title: 'Zone',
                          subtitle: 'Mähbereich ergänzen',
                          detail: '${controller.zoneCount} Zonen',
                          enabled: canCreateZone,
                          onTap: () {
                            controller.createMapObject(
                              EditableMapObjectType.zone,
                            );
                            context.pop();
                          },
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
            Positioned(
              left: 12,
              bottom: 24 + bottomInset,
              child: _MapSizeProgressPill(
                currentSquareMeters: controller.mapAreaSquareMeters,
                maxSquareMeters: 5000,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _RoundCloseButton extends StatelessWidget {
  const _RoundCloseButton({required this.onTap});

  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xFF3A3A3C),
      shape: const CircleBorder(),
      child: InkWell(
        customBorder: const CircleBorder(),
        onTap: onTap,
        child: const Padding(
          padding: EdgeInsets.all(12),
          child: Icon(Icons.close_rounded, size: 20, color: Colors.white),
        ),
      ),
    );
  }
}

class _ObjectTile extends StatelessWidget {
  const _ObjectTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.detail,
    required this.onTap,
    this.enabled = true,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final String detail;
  final VoidCallback onTap;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: enabled ? Colors.white : const Color(0xFFF2F2F6),
      borderRadius: BorderRadius.circular(18),
      child: InkWell(
        borderRadius: BorderRadius.circular(18),
        onTap: enabled ? onTap : null,
        child: Padding(
          padding: const EdgeInsets.all(18),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Container(
                width: 52,
                height: 52,
                decoration: BoxDecoration(
                  color: enabled
                      ? const Color(0xFFF2F2F6)
                      : const Color(0xFFE3E5E8),
                  borderRadius: BorderRadius.circular(16),
                ),
                child: Icon(
                  icon,
                  size: 28,
                  color: enabled
                      ? const Color(0xFF2F7D4A)
                      : const Color(0xFF8A8A90),
                ),
              ),
              const Spacer(),
              Text(
                title,
                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  color: enabled
                      ? const Color(0xFF1C1C1E)
                      : const Color(0xFF6C6C70),
                  fontWeight: FontWeight.w800,
                ),
              ),
              const SizedBox(height: 6),
              Text(
                subtitle,
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: enabled
                      ? const Color(0xFF3A3A3C)
                      : const Color(0xFF8A8A90),
                ),
              ),
              const SizedBox(height: 8),
              Text(
                detail,
                style: Theme.of(
                  context,
                ).textTheme.bodySmall?.copyWith(color: const Color(0xFF6C6C70)),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _MapSizeProgressPill extends StatelessWidget {
  const _MapSizeProgressPill({
    required this.currentSquareMeters,
    required this.maxSquareMeters,
  });

  final int currentSquareMeters;
  final int maxSquareMeters;

  @override
  Widget build(BuildContext context) {
    final progress = maxSquareMeters == 0
        ? 0.0
        : (currentSquareMeters / maxSquareMeters).clamp(0.0, 1.0);

    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF2FFFFFF),
        borderRadius: BorderRadius.circular(18),
        boxShadow: const <BoxShadow>[
          BoxShadow(
            color: Color(0x14000000),
            blurRadius: 12,
            offset: Offset(0, 4),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            SizedBox(
              width: 116,
              child: ClipRRect(
                borderRadius: BorderRadius.circular(999),
                child: LinearProgressIndicator(
                  value: progress,
                  minHeight: 6,
                  backgroundColor: const Color(0xFFD8D8DD),
                  valueColor: const AlwaysStoppedAnimation<Color>(
                    Color(0xFF2F7D4A),
                  ),
                ),
              ),
            ),
            const SizedBox(height: 10),
            RichText(
              text: TextSpan(
                style: Theme.of(
                  context,
                ).textTheme.bodySmall?.copyWith(color: const Color(0xFF6C6C70)),
                children: <TextSpan>[
                  const TextSpan(text: 'Kartengröße '),
                  TextSpan(
                    text: '$currentSquareMeters',
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFF1C1C1E),
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  TextSpan(text: ' / $maxSquareMeters m²'),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
