import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/robot/robot_status.dart';
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
              child: Stack(
                children: <Widget>[
                  ClipRRect(
                    borderRadius: BorderRadius.circular(20),
                    child: DecoratedBox(
                      decoration: BoxDecoration(
                        color: const Color(0xFF08101D),
                        borderRadius: BorderRadius.circular(20),
                        border: Border.all(color: Theme.of(context).dividerColor),
                      ),
                      child: RobotMapView(
                        map: geometry,
                        status: status.connectionState == ConnectionStateKind.connected ? status : null,
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
                        showCenterButton: editorState.mode == MapEditorMode.view,
                      ),
                    ),
                  ),
                  if (editorState.mode == MapEditorMode.record)
                    Positioned(
                      left: 0,
                      right: 0,
                      bottom: 0,
                      child: _RecordPanel(
                        status: status,
                        activePoints: activePoints,
                        onSavePoint: () {
                          final lat = status.gpsLat;
                          final lon = status.gpsLon;
                          if (lat == null || lon == null) return;
                          HapticFeedback.lightImpact();
                          _handleMapTap(ref, geometry, editorState, MapPoint(x: lon, y: lat));
                        },
                        onClose: activePoints.length >= 3
                            ? () {
                                ref.read(mapEditorStateProvider.notifier).state =
                                    editorState.copyWith(mode: MapEditorMode.view, clearSelectedPoint: true);
                              }
                            : null,
                        onCancel: () => _handleCancelRecording(context, ref, geometry, editorState, controller),
                      ),
                    ),
                ],
              ),
            ),
          ),
          if (editorState.mode != MapEditorMode.record)
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

  Future<void> _handleCancelRecording(
    BuildContext context,
    WidgetRef ref,
    MapGeometry geometry,
    MapEditorState editorState,
    dynamic controller,
  ) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Aufzeichnung abbrechen'),
        content: const Text('Alle aufgezeichneten Punkte werden verworfen.'),
        actions: <Widget>[
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Weiter aufzeichnen'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Verwerfen'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    ref.read(mapGeometryProvider.notifier).state =
        controller.deleteActiveObject(geometry, editorState);
    ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
      mode: MapEditorMode.view,
      clearSelectedPoint: true,
      clearActiveZone: editorState.activeObject == EditableMapObjectType.zone,
      clearActiveNoGo: editorState.activeObject == EditableMapObjectType.noGo,
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

class _RecordPanel extends StatelessWidget {
  const _RecordPanel({
    required this.status,
    required this.activePoints,
    required this.onSavePoint,
    required this.onClose,
    required this.onCancel,
  });

  final RobotStatus status;
  final List<MapPoint> activePoints;
  final VoidCallback onSavePoint;
  final VoidCallback? onClose;
  final VoidCallback onCancel;

  @override
  Widget build(BuildContext context) {
    final hasGps = status.connectionState == ConnectionStateKind.connected &&
        status.gpsLat != null &&
        status.gpsLon != null;
    final lastPoint = activePoints.isNotEmpty ? activePoints.last : null;

    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF0F1829),
        border: Border(top: BorderSide(color: Theme.of(context).dividerColor)),
        borderRadius: const BorderRadius.vertical(top: Radius.circular(16)),
      ),
      padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: <Widget>[
          // Point counter + last coords
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: <Widget>[
              Text(
                '${activePoints.length} Punkt${activePoints.length == 1 ? '' : 'e'}',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFF60A5FA),
                      fontWeight: FontWeight.w600,
                    ),
              ),
              if (lastPoint != null)
                Text(
                  '${lastPoint.y.toStringAsFixed(6)}, ${lastPoint.x.toStringAsFixed(6)}',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        fontFamily: 'monospace',
                        color: const Color(0xFF64748B),
                        fontSize: 11,
                      ),
                ),
            ],
          ),
          const SizedBox(height: 10),
          // Primary button
          FilledButton.icon(
            onPressed: hasGps ? onSavePoint : null,
            style: FilledButton.styleFrom(
              backgroundColor: hasGps ? const Color(0xFF16A34A) : null,
              padding: const EdgeInsets.symmetric(vertical: 14),
            ),
            icon: const Icon(Icons.add_location_alt_rounded),
            label: Text(
              hasGps ? '+ Punkt speichern' : 'Kein GPS-Signal',
              style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w700),
            ),
          ),
          const SizedBox(height: 10),
          // Secondary actions
          Row(
            children: <Widget>[
              Expanded(
                child: OutlinedButton(
                  onPressed: onClose,
                  style: OutlinedButton.styleFrom(
                    side: BorderSide(
                      color: onClose != null
                          ? const Color(0xFF2563EB)
                          : const Color(0xFF334155),
                    ),
                  ),
                  child: Column(
                    children: <Widget>[
                      const Text('Schliessen'),
                      if (activePoints.length < 3)
                        Text(
                          'min. 3 Punkte',
                          style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 10),
                        ),
                    ],
                  ),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: OutlinedButton(
                  onPressed: onCancel,
                  style: OutlinedButton.styleFrom(
                    side: const BorderSide(color: Color(0xFF7C3AED)),
                    foregroundColor: const Color(0xFFC4B5FD),
                  ),
                  child: const Text('Abbrechen'),
                ),
              ),
            ],
          ),
        ],
      ),
    );
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
