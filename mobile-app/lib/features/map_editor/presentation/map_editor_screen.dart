import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/robot/saved_robot.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/robot_map_view.dart';
import '../../../shared/widgets/section_card.dart';
import '../models/map_editor_state.dart';

class MapEditorScreen extends ConsumerWidget {
  const MapEditorScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final geometry = ref.watch(mapGeometryProvider);
    final editorState = ref.watch(mapEditorStateProvider);
    final controller = ref.watch(mapEditorControllerProvider);
    final status = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final activePoints = controller.activePoints(geometry, editorState);
    final segmentPoints = _segmentTargets(activePoints);
    final activeObjectLabel = _activeObjectLabel(geometry, editorState);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Karte'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Karte speichern',
            onPressed: () => _saveMap(context, ref, geometry, activeRobot),
            icon: const Icon(Icons.save_outlined),
          ),
          PopupMenuButton<String>(
            tooltip: 'Layer',
            onSelected: (value) {
              final current = ref.read(mapEditorStateProvider);
              switch (value) {
                case 'perimeter':
                  ref.read(mapEditorStateProvider.notifier).state = current.copyWith(
                        showPerimeter: !current.showPerimeter,
                      );
                case 'nogo':
                  ref.read(mapEditorStateProvider.notifier).state = current.copyWith(
                        showNoGo: !current.showNoGo,
                      );
                case 'zones':
                  ref.read(mapEditorStateProvider.notifier).state = current.copyWith(
                        showZones: !current.showZones,
                      );
                case 'dock':
                  ref.read(mapEditorStateProvider.notifier).state = current.copyWith(
                        showDock: !current.showDock,
                      );
              }
            },
            itemBuilder: (context) => <PopupMenuEntry<String>>[
              CheckedPopupMenuItem<String>(
                value: 'perimeter',
                checked: editorState.showPerimeter,
                child: const Text('Perimeter'),
              ),
              CheckedPopupMenuItem<String>(
                value: 'nogo',
                checked: editorState.showNoGo,
                child: const Text('No-Go'),
              ),
              CheckedPopupMenuItem<String>(
                value: 'zones',
                checked: editorState.showZones,
                child: const Text('Zonen'),
              ),
              CheckedPopupMenuItem<String>(
                value: 'dock',
                checked: editorState.showDock,
                child: const Text('Dock'),
              ),
            ],
            icon: const Icon(Icons.layers_outlined),
          ),
          PopupMenuButton<EditableMapObjectType>(
            tooltip: 'Neues Objekt',
            onSelected: (value) {
              _createObject(ref, geometry, value);
            },
            itemBuilder: (context) => const <PopupMenuEntry<EditableMapObjectType>>[
              PopupMenuItem(
                value: EditableMapObjectType.perimeter,
                child: Text('+ Perimeter'),
              ),
              PopupMenuItem(
                value: EditableMapObjectType.noGo,
                child: Text('+ No-Go-Zone'),
              ),
              PopupMenuItem(
                value: EditableMapObjectType.zone,
                child: Text('+ Mähzone'),
              ),
              PopupMenuItem(
                value: EditableMapObjectType.dock,
                child: Text('+ Dock setzen'),
              ),
            ],
            icon: const Icon(Icons.add_circle_outline_rounded),
          ),
        ],
      ),
      body: Column(
        children: <Widget>[
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 0),
            child: ConnectionNotice(status: status),
          ),
          Expanded(
            child: Padding(
              padding: EdgeInsets.fromLTRB(
                16,
                status.lastError == null ? 16 : 12,
                16,
                0,
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(20),
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: const Color(0xFF08101D),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(color: Theme.of(context).dividerColor),
                  ),
                  child: RobotMapView(
                    map: geometry,
                    interactive: true,
                    onMapTap: (point) => _handleMapTap(ref, geometry, editorState, point),
                    activePoints: activePoints,
                    segmentPoints: editorState.mode == MapEditorMode.edit ? segmentPoints : const <EditableSegmentTarget>[],
                    onPointTap: (index) {
                      final current = ref.read(mapEditorStateProvider);
                      ref.read(mapEditorStateProvider.notifier).state = current.copyWith(
                            selectedPointIndex: index,
                          );
                    },
                    onSegmentTap: (insertIndex) {
                      final current = ref.read(mapEditorStateProvider);
                      final target = segmentPoints.firstWhere((segment) => segment.insertIndex == insertIndex);
                      final nextGeometry = controller.insertPoint(geometry, current, insertIndex, target.point);
                      ref.read(mapGeometryProvider.notifier).state = nextGeometry;
                    },
                    selectedPointIndex: editorState.selectedPointIndex,
                    showPerimeter: editorState.showPerimeter,
                    showNoGo: editorState.showNoGo,
                    showZones: editorState.showZones,
                    showDock: editorState.showDock,
                    highlightActiveZoneId: editorState.activeObject == EditableMapObjectType.zone
                        ? editorState.activeZoneId
                        : null,
                  ),
                ),
              ),
            ),
          ),
          ConstrainedBox(
            constraints: const BoxConstraints(maxHeight: 280),
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: <Widget>[
                SectionCard(
                  title: 'Bearbeitungsmodus',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      SegmentedButton<MapEditorMode>(
                        segments: const <ButtonSegment<MapEditorMode>>[
                          ButtonSegment<MapEditorMode>(value: MapEditorMode.view, label: Text('Ansicht')),
                          ButtonSegment<MapEditorMode>(value: MapEditorMode.record, label: Text('Aufzeichnen')),
                          ButtonSegment<MapEditorMode>(value: MapEditorMode.edit, label: Text('Bearbeiten')),
                        ],
                        selected: <MapEditorMode>{editorState.mode},
                        onSelectionChanged: (selection) {
                          ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                                mode: selection.first,
                                clearSelectedPoint: true,
                              );
                        },
                      ),
                      const SizedBox(height: 12),
                      Text(
                        'Aktiv: $activeObjectLabel',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 6),
                      Text(
                        _hintText(editorState),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                SectionCard(
                  title: 'Objekte',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      _ObjectChipRow(
                        label: 'Perimeter',
                        selected: editorState.activeObject == EditableMapObjectType.perimeter,
                        onTap: () => ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                              activeObject: EditableMapObjectType.perimeter,
                              clearActiveZone: true,
                              clearActiveNoGo: true,
                              clearSelectedPoint: true,
                            ),
                      ),
                      const SizedBox(height: 8),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: geometry.zones
                            .map(
                              (zone) => FilterChip(
                                label: Text(zone.name),
                                selected: editorState.activeObject == EditableMapObjectType.zone &&
                                    editorState.activeZoneId == zone.id,
                                onSelected: (_) {
                                  ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                                        activeObject: EditableMapObjectType.zone,
                                        activeZoneId: zone.id,
                                        clearActiveNoGo: true,
                                        clearSelectedPoint: true,
                                      );
                                },
                              ),
                            )
                            .toList(growable: false),
                      ),
                      if (geometry.noGoZones.isNotEmpty) ...<Widget>[
                        const SizedBox(height: 8),
                        Wrap(
                          spacing: 8,
                          runSpacing: 8,
                          children: List<Widget>.generate(
                            geometry.noGoZones.length,
                            (index) => FilterChip(
                              label: Text('No-Go ${index + 1}'),
                              selected: editorState.activeObject == EditableMapObjectType.noGo &&
                                  editorState.activeNoGoIndex == index,
                              onSelected: (_) {
                                ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                                      activeObject: EditableMapObjectType.noGo,
                                      activeNoGoIndex: index,
                                      clearActiveZone: true,
                                      clearSelectedPoint: true,
                                    );
                              },
                            ),
                          ),
                        ),
                      ],
                      const SizedBox(height: 12),
                      Row(
                        children: <Widget>[
                          Expanded(
                            child: OutlinedButton(
                              onPressed: editorState.selectedPointIndex == null
                                  ? null
                                  : () {
                                      final nextGeometry = controller.deletePoint(
                                        geometry,
                                        editorState,
                                        editorState.selectedPointIndex!,
                                      );
                                      ref.read(mapGeometryProvider.notifier).state = nextGeometry;
                                      ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                                            clearSelectedPoint: true,
                                          );
                                    },
                              child: const Text('Punkt löschen'),
                            ),
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: FilledButton.tonal(
                              onPressed: () {
                                ref.read(mapGeometryProvider.notifier).state =
                                    controller.deleteActiveObject(geometry, editorState);
                                ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                                      clearSelectedPoint: true,
                                      clearActiveZone: editorState.activeObject == EditableMapObjectType.zone,
                                      clearActiveNoGo: editorState.activeObject == EditableMapObjectType.noGo,
                                    );
                              },
                              child: const Text('Objekt löschen'),
                            ),
                          ),
                        ],
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

  Future<void> _saveMap(
    BuildContext context,
    WidgetRef ref,
    MapGeometry geometry,
    SavedRobot? activeRobot,
  ) async {
    try {
      await ref.read(appStateStorageProvider).saveMapGeometry(geometry);
      if (activeRobot != null) {
        await ref.read(robotRepositoryProvider).saveMapGeometry(
              host: activeRobot.lastHost,
              port: activeRobot.port,
              geometry: geometry,
            );
      }
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              activeRobot == null
                  ? 'Karte lokal gespeichert.'
                  : 'Karte auf Alfred gespeichert.',
            ),
          ),
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Karte konnte nicht gespeichert werden: $error')),
        );
      }
    }
  }

  void _createObject(
    WidgetRef ref,
    MapGeometry geometry,
    EditableMapObjectType value,
  ) {
    final controller = ref.read(mapEditorControllerProvider);
    switch (value) {
      case EditableMapObjectType.perimeter:
        ref.read(mapEditorStateProvider.notifier).state = const MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.perimeter,
        );
      case EditableMapObjectType.noGo:
        final next = controller.addNoGo(geometry);
        ref.read(mapGeometryProvider.notifier).state = next;
        ref.read(mapEditorStateProvider.notifier).state = MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.noGo,
          activeNoGoIndex: next.noGoZones.length - 1,
        );
      case EditableMapObjectType.zone:
        final next = controller.addZone(geometry);
        final zoneId = next.zones.isEmpty ? null : next.zones.last.id;
        ref.read(mapGeometryProvider.notifier).state = next;
        ref.read(mapEditorStateProvider.notifier).state = MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.zone,
          activeZoneId: zoneId,
        );
      case EditableMapObjectType.dock:
        ref.read(mapEditorStateProvider.notifier).state = const MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.dock,
        );
    }
  }

  void _handleMapTap(
    WidgetRef ref,
    MapGeometry geometry,
    MapEditorState editorState,
    MapPoint point,
  ) {
    if (!editorState.canEditPoints) return;
    final controller = ref.read(mapEditorControllerProvider);

    if (editorState.mode == MapEditorMode.edit && editorState.selectedPointIndex != null) {
      final next = controller.movePoint(
        geometry,
        editorState,
        editorState.selectedPointIndex!,
        point,
      );
      ref.read(mapGeometryProvider.notifier).state = next;
      ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(clearSelectedPoint: true);
      return;
    }

    final next = controller.addPoint(geometry, editorState, point);
    ref.read(mapGeometryProvider.notifier).state = next;
  }

  String _activeObjectLabel(MapGeometry geometry, MapEditorState state) {
    switch (state.activeObject) {
      case EditableMapObjectType.perimeter:
        return 'Perimeter';
      case EditableMapObjectType.dock:
        return 'Dock';
      case EditableMapObjectType.noGo:
        final index = state.activeNoGoIndex;
        return index == null ? 'No-Go' : 'No-Go ${index + 1}';
      case EditableMapObjectType.zone:
        final zone = geometry.zones.where((entry) => entry.id == state.activeZoneId).cast<ZoneGeometry?>().firstWhere(
              (zone) => zone != null,
              orElse: () => null,
            );
        return zone?.name ?? 'Zone';
    }
  }

  String _hintText(MapEditorState state) {
    switch (state.mode) {
      case MapEditorMode.view:
        return 'Karte verschieben/zoomen und Objekte auswählen.';
      case MapEditorMode.record:
        return 'Tippe in die Karte, um Punkte zum aktiven Objekt hinzuzufügen.';
      case MapEditorMode.edit:
        return 'Punkt antippen und danach neues Ziel in der Karte tippen. Plus-Markierungen fügen Punkte in Segmente ein.';
    }
  }

  List<EditableSegmentTarget> _segmentTargets(List<MapPoint> points) {
    if (points.length < 2) return const <EditableSegmentTarget>[];
    final targets = <EditableSegmentTarget>[];
    for (var index = 0; index < points.length - 1; index += 1) {
      final current = points[index];
      final next = points[index + 1];
      targets.add(
        EditableSegmentTarget(
          insertIndex: index + 1,
          point: MapPoint(
            x: (current.x + next.x) / 2,
            y: (current.y + next.y) / 2,
          ),
        ),
      );
    }
    if (points.length >= 3) {
      final current = points.last;
      final next = points.first;
      targets.add(
        EditableSegmentTarget(
          insertIndex: points.length,
          point: MapPoint(
            x: (current.x + next.x) / 2,
            y: (current.y + next.y) / 2,
          ),
        ),
      );
    }
    return targets;
  }
}

class _ObjectChipRow extends StatelessWidget {
  const _ObjectChipRow({
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Align(
      alignment: Alignment.centerLeft,
      child: ChoiceChip(
        label: Text(label),
        selected: selected,
        onSelected: (_) => onTap(),
      ),
    );
  }
}
