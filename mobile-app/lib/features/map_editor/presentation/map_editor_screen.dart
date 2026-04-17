import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../domain/robot/saved_robot.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/robot_map_view.dart';
import '../../../shared/widgets/section_card.dart';
import '../models/map_editor_state.dart';

enum MapEditorEntryFlow {
  general,
  setup,
  edit,
}

class MapEditorScreen extends ConsumerWidget {
  const MapEditorScreen({
    this.entryFlow = MapEditorEntryFlow.general,
    super.key,
  });

  final MapEditorEntryFlow entryFlow;

  // ── Undo / Redo helpers ──────────────────────────────────────────────────

  static void _setGeometry(WidgetRef ref, MapGeometry next) {
    final current = ref.read(mapGeometryProvider);
    final stack = ref.read(mapUndoStackProvider);
    ref.read(mapUndoStackProvider.notifier).state = [...stack, current];
    ref.read(mapRedoStackProvider.notifier).state = const <MapGeometry>[];
    ref.read(mapGeometryProvider.notifier).state = next;
  }

  static void _undo(WidgetRef ref) {
    final stack = ref.read(mapUndoStackProvider);
    if (stack.isEmpty) return;
    final previous = stack.last;
    final redo = ref.read(mapRedoStackProvider);
    ref.read(mapRedoStackProvider.notifier).state = [...redo, ref.read(mapGeometryProvider)];
    ref.read(mapUndoStackProvider.notifier).state = stack.sublist(0, stack.length - 1);
    ref.read(mapGeometryProvider.notifier).state = previous;
  }

  static void _redo(WidgetRef ref) {
    final stack = ref.read(mapRedoStackProvider);
    if (stack.isEmpty) return;
    final next = stack.last;
    final undo = ref.read(mapUndoStackProvider);
    ref.read(mapUndoStackProvider.notifier).state = [...undo, ref.read(mapGeometryProvider)];
    ref.read(mapRedoStackProvider.notifier).state = stack.sublist(0, stack.length - 1);
    ref.read(mapGeometryProvider.notifier).state = next;
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final geometry = ref.watch(mapGeometryProvider);
    final editorState = ref.watch(mapEditorStateProvider);
    final controller = ref.watch(mapEditorControllerProvider);
    final status = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final undoStack = ref.watch(mapUndoStackProvider);
    final redoStack = ref.watch(mapRedoStackProvider);
    final activePoints = controller.activePoints(geometry, editorState);
    final segmentPoints = _segmentTargets(activePoints);
    final activeObjectLabel = _activeObjectLabel(geometry, editorState);
    final isSetupFlow = entryFlow == MapEditorEntryFlow.setup;
    final isEditFlow = entryFlow == MapEditorEntryFlow.edit;
    final hasPerimeter = geometry.perimeter.length >= 3;
    final hasDock = geometry.dock.isNotEmpty;
    final hasZones = geometry.zones.isNotEmpty;
    final hasValidNoGo = geometry.noGoZones.every((zone) => zone.isEmpty || zone.length >= 3);
    final hasValidZones = geometry.zones.every((zone) => zone.points.isEmpty || zone.points.length >= 3);
    final hasBaseMapReady = hasPerimeter && hasDock && hasValidNoGo;
    final setupReady = hasBaseMapReady && hasValidZones;
    final setupStage = editorState.setupStage;
    final canCreateZone = hasBaseMapReady;
    final canSaveInCurrentContext = !isSetupFlow || (setupStage == MapSetupStage.save && setupReady);

    if (isEditFlow) {
      return Scaffold(
        body: Stack(
          children: <Widget>[
            Positioned.fill(
              child: RobotMapView(
                map: geometry,
                status: status.connectionState == ConnectionStateKind.connected
                    ? status
                    : null,
                interactive: true,
                onMapTap: (point) => _handleMapTap(ref, geometry, editorState, point),
                activePoints: activePoints,
                segmentPoints: editorState.mode == MapEditorMode.edit
                    ? segmentPoints
                    : const <EditableSegmentTarget>[],
                onPointTap: (index) {
                  final current = ref.read(mapEditorStateProvider);
                  ref.read(mapEditorStateProvider.notifier).state =
                      current.copyWith(selectedPointIndex: index);
                },
                onSegmentTap: (insertIndex) {
                  final current = ref.read(mapEditorStateProvider);
                  final target = segmentPoints.firstWhere(
                    (segment) => segment.insertIndex == insertIndex,
                  );
                  final nextGeometry = controller.insertPoint(
                    geometry,
                    current,
                    insertIndex,
                    target.point,
                  );
                  _setGeometry(ref, nextGeometry);
                },
                selectedPointIndex: editorState.selectedPointIndex,
                showPerimeter: editorState.showPerimeter,
                showNoGo: editorState.showNoGo,
                showZones: editorState.showZones,
                showDock: editorState.showDock,
                highlightActiveZoneId:
                    editorState.activeObject == EditableMapObjectType.zone
                        ? editorState.activeZoneId
                        : null,
                showCenterButton: editorState.mode == MapEditorMode.view,
              ),
            ),
            Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    gradient: const LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: <Color>[
                        Color(0xC608101D),
                        Color(0x0008101D),
                        Color(0x0008101D),
                        Color(0xE608101D),
                      ],
                      stops: <double>[0, 0.22, 0.55, 1],
                    ),
                  ),
                ),
              ),
            ),
            SafeArea(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    Row(
                      children: <Widget>[
                        _MapOverlayIconButton(
                          icon: Icons.arrow_back_rounded,
                          tooltip: 'Dashboard',
                          onPressed: () => context.go('/dashboard'),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: _MapOverlayTitleCard(
                            title: 'Karte bearbeiten',
                            subtitle: activeObjectLabel,
                          ),
                        ),
                        const SizedBox(width: 10),
                        _MapOverlayIconButton(
                          icon: Icons.undo_rounded,
                          tooltip: 'Rückgängig',
                          onPressed: undoStack.isEmpty ? null : () => _undo(ref),
                        ),
                        const SizedBox(width: 8),
                        _MapOverlayIconButton(
                          icon: Icons.redo_rounded,
                          tooltip: 'Wiederherstellen',
                          onPressed: redoStack.isEmpty ? null : () => _redo(ref),
                        ),
                      ],
                    ),
                    if (status.lastError != null) ...<Widget>[
                      const SizedBox(height: 12),
                      ConnectionNotice(status: status),
                    ],
                    const Spacer(),
                    Align(
                      alignment: Alignment.centerRight,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.end,
                        children: <Widget>[
                          _FloatingActionPill(
                            icon: Icons.add_circle_outline_rounded,
                            label: 'Mehr hinzufügen',
                            onTap: () => _openAddMoreSheet(context, ref, geometry),
                          ),
                          const SizedBox(height: 10),
                          _FloatingActionPill(
                            icon: editorState.mode == MapEditorMode.edit
                                ? Icons.visibility_outlined
                                : Icons.edit_location_alt_rounded,
                            label: editorState.mode == MapEditorMode.edit
                              ? 'Ansehen'
                              : 'Bearbeiten',
                            onTap: () {
                              if (editorState.mode == MapEditorMode.edit) {
                                ref.read(mapEditorStateProvider.notifier).state =
                                    editorState.copyWith(
                                      mode: MapEditorMode.view,
                                      clearSelectedPoint: true,
                                    );
                              } else {
                                _enterEditMode(ref, geometry);
                              }
                            },
                          ),
                          const SizedBox(height: 10),
                          PopupMenuButton<String>(
                            tooltip: 'Layer',
                            color: const Color(0xEE0F172A),
                            onSelected: (value) {
                              final current = ref.read(mapEditorStateProvider);
                              switch (value) {
                                case 'perimeter':
                                  ref.read(mapEditorStateProvider.notifier).state =
                                      current.copyWith(
                                        showPerimeter: !current.showPerimeter,
                                      );
                                case 'nogo':
                                  ref.read(mapEditorStateProvider.notifier).state =
                                      current.copyWith(
                                        showNoGo: !current.showNoGo,
                                      );
                                case 'zones':
                                  ref.read(mapEditorStateProvider.notifier).state =
                                      current.copyWith(
                                        showZones: !current.showZones,
                                      );
                                case 'dock':
                                  ref.read(mapEditorStateProvider.notifier).state =
                                      current.copyWith(
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
                            child: const _MapOverlayIconButton(
                              icon: Icons.layers_outlined,
                              tooltip: 'Layer',
                            ),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 14),
                    Container(
                      width: double.infinity,
                      padding: const EdgeInsets.fromLTRB(16, 16, 16, 16),
                      decoration: BoxDecoration(
                        color: const Color(0xE60F1829),
                        borderRadius: BorderRadius.circular(26),
                        border: Border.all(color: const Color(0x331E3A5F)),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: <Widget>[
                          Row(
                            children: <Widget>[
                              Expanded(
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: <Widget>[
                                    Text(
                                      activeObjectLabel,
                                      style: Theme.of(context)
                                          .textTheme
                                          .titleMedium
                                          ?.copyWith(fontWeight: FontWeight.w700),
                                    ),
                                    const SizedBox(height: 4),
                                    Text(
                                      _hintText(editorState),
                                      style:
                                          Theme.of(context).textTheme.bodySmall,
                                    ),
                                  ],
                                ),
                              ),
                              const SizedBox(width: 12),
                              _MapOverlayIconButton(
                                icon: Icons.save_outlined,
                                tooltip: 'Karte speichern',
                                onPressed: canSaveInCurrentContext
                                    ? () => _saveMap(
                                          context,
                                          ref,
                                          geometry,
                                          activeRobot,
                                        )
                                    : null,
                              ),
                            ],
                          ),
                          const SizedBox(height: 14),
                          SingleChildScrollView(
                            scrollDirection: Axis.horizontal,
                            child: Row(
                              children: <Widget>[
                                _FocusPill(
                                  label:
                                      'Perimeter ${hasPerimeter ? 'bereit' : 'offen'}',
                                ),
                                const SizedBox(width: 8),
                                _FocusPill(
                                  label: 'Dock ${hasDock ? 'bereit' : 'offen'}',
                                ),
                                const SizedBox(width: 8),
                                _FocusPill(label: '${geometry.zones.length} Zonen'),
                                const SizedBox(width: 8),
                                _FocusPill(
                                  label: '${geometry.noGoZones.length} No-Go',
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(height: 14),
                          if (geometry.zones.isNotEmpty)
                            SingleChildScrollView(
                              scrollDirection: Axis.horizontal,
                              child: Row(
                                children: geometry.zones
                                    .map(
                                      (zone) => Padding(
                                        padding:
                                            const EdgeInsets.only(right: 8),
                                        child: FilterChip(
                                          label: Text(zone.name),
                                          selected: editorState.activeObject ==
                                                  EditableMapObjectType.zone &&
                                              editorState.activeZoneId ==
                                                  zone.id,
                                          onSelected: (_) {
                                            ref
                                                .read(
                                                  mapEditorStateProvider.notifier,
                                                )
                                                .state = editorState.copyWith(
                                                  mode: MapEditorMode.edit,
                                                  activeObject:
                                                      EditableMapObjectType.zone,
                                                  activeZoneId: zone.id,
                                                  clearActiveNoGo: true,
                                                  clearSelectedPoint: true,
                                                );
                                          },
                                        ),
                                      ),
                                    )
                                    .toList(growable: false),
                              ),
                            ),
                          if (geometry.zones.isNotEmpty) const SizedBox(height: 14),
                          Row(
                            children: <Widget>[
                              if (editorState.activeObject ==
                                      EditableMapObjectType.zone &&
                                  editorState.activeZoneId != null) ...<Widget>[
                                Expanded(
                                  child: OutlinedButton.icon(
                                    onPressed: () => _configureActiveZone(
                                      context,
                                      ref,
                                      geometry,
                                      editorState.activeZoneId!,
                                    ),
                                    icon: const Icon(Icons.tune_rounded),
                                    label: const Text('Zone'),
                                  ),
                                ),
                                const SizedBox(width: 10),
                              ],
                              Expanded(
                                child: OutlinedButton(
                                  onPressed: editorState.selectedPointIndex == null
                                      ? null
                                      : () {
                                          final nextGeometry =
                                              controller.deletePoint(
                                            geometry,
                                            editorState,
                                            editorState.selectedPointIndex!,
                                          );
                                          _setGeometry(ref, nextGeometry);
                                          ref
                                              .read(
                                                mapEditorStateProvider.notifier,
                                              )
                                              .state = editorState.copyWith(
                                                clearSelectedPoint: true,
                                              );
                                        },
                                  child: const Text('Punkt löschen'),
                                ),
                              ),
                              const SizedBox(width: 10),
                              Expanded(
                                child: FilledButton.tonal(
                                  onPressed: () {
                                    _setGeometry(
                                      ref,
                                      controller.deleteActiveObject(
                                        geometry,
                                        editorState,
                                      ),
                                    );
                                    ref.read(mapEditorStateProvider.notifier).state =
                                        editorState.copyWith(
                                      clearSelectedPoint: true,
                                      clearActiveZone:
                                          editorState.activeObject ==
                                              EditableMapObjectType.zone,
                                      clearActiveNoGo:
                                          editorState.activeObject ==
                                              EditableMapObjectType.noGo,
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
                    if (editorState.mode == MapEditorMode.record) ...<Widget>[
                      const SizedBox(height: 12),
                      _RecordPanel(
                        status: status,
                        activePoints: activePoints,
                        onSavePoint: () {
                          final lat = status.gpsLat;
                          final lon = status.gpsLon;
                          if (lat == null || lon == null) return;
                          HapticFeedback.lightImpact();
                          _handleMapTap(
                            ref,
                            geometry,
                            editorState,
                            MapPoint(x: lon, y: lat),
                          );
                        },
                        onClose: activePoints.length >= 3
                            ? () {
                                ref.read(mapEditorStateProvider.notifier).state =
                                    editorState.copyWith(
                                      mode: MapEditorMode.view,
                                      clearSelectedPoint: true,
                                    );
                              }
                            : null,
                        onCancel: () => _handleCancelRecording(
                          context,
                          ref,
                          geometry,
                          editorState,
                          controller,
                        ),
                      ),
                    ],
                  ],
                ),
              ),
            ),
          ],
        ),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: Text(
          isSetupFlow
              ? 'Kartensetup'
              : isEditFlow
                  ? 'Karte bearbeiten'
                  : 'Karte',
        ),
        actions: <Widget>[
          IconButton(
            tooltip: 'Dashboard',
            onPressed: () => context.go('/dashboard'),
            icon: const Icon(Icons.space_dashboard_outlined),
          ),
          IconButton(
            tooltip: 'Rückgängig',
            onPressed: undoStack.isEmpty ? null : () => _undo(ref),
            icon: const Icon(Icons.undo_rounded),
          ),
          IconButton(
            tooltip: 'Wiederherstellen',
            onPressed: redoStack.isEmpty ? null : () => _redo(ref),
            icon: const Icon(Icons.redo_rounded),
          ),
          IconButton(
            tooltip: 'Karte speichern',
            onPressed: canSaveInCurrentContext
                ? () => _saveMap(context, ref, geometry, activeRobot)
                : null,
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
          if (!isSetupFlow)
            PopupMenuButton<EditableMapObjectType>(
              tooltip: 'Neues Objekt',
              onSelected: (value) {
                _createObject(ref, geometry, value);
              },
              itemBuilder: (context) => <PopupMenuEntry<EditableMapObjectType>>[
                const PopupMenuItem(
                  value: EditableMapObjectType.perimeter,
                  child: Text('+ Perimeter'),
                ),
                const PopupMenuItem(
                  value: EditableMapObjectType.noGo,
                  child: Text('+ No-Go-Zone'),
                ),
                const PopupMenuItem(
                  value: EditableMapObjectType.dock,
                  child: Text('+ Docking setzen'),
                ),
                PopupMenuItem(
                  value: EditableMapObjectType.zone,
                  enabled: canCreateZone,
                  child: const Text('+ Mähzone'),
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
                          _setGeometry(ref, nextGeometry);
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
                if (isSetupFlow) ...<Widget>[
                SectionCard(
                  title: 'Basiskarten-Werkzeugleiste',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(
                        'Lege die Basiskarte immer in dieser Reihenfolge an: Perimeter, NoGo, Docking. Zonen sind erst danach ein optionaler Schritt.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 12),
                      Wrap(
                        spacing: 10,
                        runSpacing: 10,
                        children: <Widget>[
                          _BaseToolChip(
                            label: '1 Perimeter',
                            subtitle: hasPerimeter ? 'bereit' : 'Pflicht',
                            selected: setupStage == MapSetupStage.perimeter,
                            enabled: true,
                            done: hasPerimeter,
                            onTap: () => _setSetupStage(ref, editorState, MapSetupStage.perimeter),
                          ),
                          _BaseToolChip(
                            label: '2 NoGo',
                            subtitle: geometry.noGoZones.isEmpty
                                ? 'optional in Basiskarte'
                                : hasValidNoGo
                                    ? 'bereit'
                                    : 'offen',
                            selected: setupStage == MapSetupStage.noGo,
                            enabled: hasPerimeter,
                            done: hasValidNoGo,
                            onTap: () => _setSetupStage(ref, editorState, MapSetupStage.noGo),
                          ),
                          _BaseToolChip(
                            label: '3 Docking',
                            subtitle: hasDock ? 'bereit' : 'Pflicht',
                            selected: setupStage == MapSetupStage.dock,
                            enabled: hasPerimeter,
                            done: hasDock,
                            onTap: () => _setSetupStage(ref, editorState, MapSetupStage.dock),
                          ),
                          _BaseToolChip(
                            label: 'Zonen',
                            subtitle: hasBaseMapReady
                                ? 'optional danach'
                                : 'erst nach Perimeter, NoGo und Docking',
                            selected: false,
                            enabled: hasBaseMapReady,
                            done: hasZones,
                            optional: true,
                            onTap: () => _createObject(ref, geometry, EditableMapObjectType.zone),
                          ),
                        ],
                      ),
                      const SizedBox(height: 12),
                      Text(
                        _setupStageDescription(setupStage),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 12),
                      _FlowStatusRow(
                        done: hasPerimeter,
                        title: 'Perimeter',
                        subtitle: hasPerimeter
                            ? 'Außenkante ist vorhanden.'
                            : 'Lege zuerst die Außenkante des Gartens an.',
                      ),
                      const SizedBox(height: 8),
                      _FlowStatusRow(
                        done: hasDock,
                        title: 'Dock',
                        subtitle: hasDock
                            ? 'Ladestation ist gesetzt.'
                            : 'Setze danach den Dock-Punkt.',
                      ),
                      const SizedBox(height: 8),
                      _FlowStatusRow(
                        done: hasZones,
                        title: 'Zonen',
                        subtitle: hasZones
                            ? '${geometry.zones.length} Zone(n) vorhanden.'
                            : 'Zonen kommen nach dem Kartensetup im Bearbeitungsflow.',
                      ),
                      const SizedBox(height: 12),
                      _SetupStageCard(
                        stage: setupStage,
                        setupReady: setupReady,
                        hasPerimeter: hasPerimeter,
                        hasDock: hasDock,
                        hasValidNoGo: hasValidNoGo,
                        onPrimaryAction: () {
                          switch (setupStage) {
                            case MapSetupStage.perimeter:
                              _createObject(ref, geometry, EditableMapObjectType.perimeter);
                            case MapSetupStage.noGo:
                              _createObject(ref, geometry, EditableMapObjectType.noGo);
                            case MapSetupStage.dock:
                              _createObject(ref, geometry, EditableMapObjectType.dock);
                            case MapSetupStage.validate:
                              _setSetupStage(ref, editorState, MapSetupStage.save);
                            case MapSetupStage.save:
                              _saveMap(context, ref, geometry, activeRobot);
                          }
                        },
                        onSecondaryAction: () {
                          switch (setupStage) {
                            case MapSetupStage.perimeter:
                              break;
                            case MapSetupStage.noGo:
                              _setSetupStage(ref, editorState, MapSetupStage.dock);
                            case MapSetupStage.dock:
                              _setSetupStage(ref, editorState, MapSetupStage.validate);
                            case MapSetupStage.validate:
                              _setSetupStage(ref, editorState, MapSetupStage.noGo);
                            case MapSetupStage.save:
                              context.go('/map/edit');
                          }
                        },
                      ),
                      if (setupStage == MapSetupStage.validate || setupStage == MapSetupStage.save) ...<Widget>[
                        const SizedBox(height: 12),
                        _ValidationList(
                          items: <_ValidationItem>[
                            _ValidationItem(
                              done: hasPerimeter,
                              label: 'Perimeter ist geschlossen genug für die Karte.',
                            ),
                            _ValidationItem(
                              done: hasValidNoGo,
                              label: 'Alle No-Go-Bereiche sind vollständig oder leer.',
                            ),
                            _ValidationItem(
                              done: hasDock,
                              label: 'Dock ist gesetzt.',
                            ),
                            _ValidationItem(
                              done: hasValidZones,
                              label: 'Vorhandene Zonen sind geometrisch vollständig.',
                            ),
                          ],
                        ),
                      ],
                      const SizedBox(height: 8),
                      Row(
                        children: <Widget>[
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: setupReady
                                  ? () => _setSetupStage(ref, editorState, MapSetupStage.save)
                                  : null,
                              icon: const Icon(Icons.task_alt_rounded),
                              label: const Text('Validierung'),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                ],
                if (isEditFlow) ...<Widget>[
                SectionCard(
                  title: 'Kartenaktionen',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(
                        'Im Normalzustand bleibt die Karte ruhig. Neue Elemente kommen über Mehr hinzufügen, Korrekturen über Bearbeiten.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: <Widget>[
                          Expanded(
                            child: FilledButton.tonalIcon(
                              onPressed: () => _openAddMoreSheet(context, ref, geometry),
                              icon: const Icon(Icons.add_circle_outline_rounded),
                              label: const Text('Mehr hinzufügen'),
                            ),
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: FilledButton.icon(
                              onPressed: () => _enterEditMode(ref, geometry),
                              icon: const Icon(Icons.edit_location_alt_rounded),
                              label: const Text('Bearbeiten'),
                            ),
                          ),
                        ],
                      ),
                      if (editorState.mode == MapEditorMode.edit) ...<Widget>[
                        const SizedBox(height: 12),
                        Text(
                          'Bearbeiten ist aktiv. Wähle ein Objekt und verschiebe oder lösche Punkte gezielt.',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                        const SizedBox(height: 12),
                        Wrap(
                          spacing: 8,
                          runSpacing: 8,
                          children: <Widget>[
                            _EditShortcutChip(
                              label: 'Perimeter',
                              enabled: hasPerimeter,
                              onTap: () => _selectObjectForEditing(
                                ref,
                                editorState.copyWith(
                                  mode: MapEditorMode.edit,
                                  activeObject: EditableMapObjectType.perimeter,
                                  clearActiveZone: true,
                                  clearActiveNoGo: true,
                                ),
                              ),
                            ),
                            _EditShortcutChip(
                              label: 'Dock',
                              enabled: hasDock,
                              onTap: () => _selectObjectForEditing(
                                ref,
                                editorState.copyWith(
                                  mode: MapEditorMode.edit,
                                  activeObject: EditableMapObjectType.dock,
                                  clearActiveZone: true,
                                  clearActiveNoGo: true,
                                ),
                              ),
                            ),
                            _EditShortcutChip(
                              label: 'Zone',
                              enabled: hasZones,
                              onTap: () {
                                final firstZoneId = geometry.zones.isEmpty ? null : geometry.zones.first.id;
                                if (firstZoneId == null) return;
                                _selectObjectForEditing(
                                  ref,
                                  editorState.copyWith(
                                    mode: MapEditorMode.edit,
                                    activeObject: EditableMapObjectType.zone,
                                    activeZoneId: firstZoneId,
                                    clearActiveNoGo: true,
                                  ),
                                );
                              },
                            ),
                            _EditShortcutChip(
                              label: 'No-Go',
                              enabled: geometry.noGoZones.isNotEmpty,
                              onTap: () => _selectObjectForEditing(
                                ref,
                                editorState.copyWith(
                                  mode: MapEditorMode.edit,
                                  activeObject: EditableMapObjectType.noGo,
                                  activeNoGoIndex: 0,
                                  clearActiveZone: true,
                                ),
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 8),
                        Align(
                          alignment: Alignment.centerRight,
                          child: TextButton.icon(
                            onPressed: () => ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
                                  mode: MapEditorMode.view,
                                  clearSelectedPoint: true,
                                ),
                            icon: const Icon(Icons.check_rounded),
                            label: const Text('Bearbeiten beenden'),
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                ],
                SectionCard(
                  title: isSetupFlow
                      ? 'Setup-Fokus'
                      : isEditFlow
                          ? 'Bearbeitungs-Fokus'
                          : 'Arbeitsbereich',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(
                        isSetupFlow
                          ? 'Lege zuerst Perimeter, No-Go-Bereiche und Dock an. Danach wird die Karte validiert und gespeichert.'
                            : isEditFlow
                                ? 'Nutze Bearbeiten für Punktkorrekturen und prüfe einzelne Layer gezielt.'
                                : 'Wechsle je nach Aufgabe zwischen Ansicht, Aufzeichnen und Bearbeiten.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 12),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: <Widget>[
                          _FocusPill(
                            label: 'Perimeter ${geometry.perimeter.length >= 3 ? 'bereit' : 'offen'}',
                          ),
                          _FocusPill(
                            label: 'Dock ${geometry.dock.isNotEmpty ? 'bereit' : 'offen'}',
                          ),
                          _FocusPill(
                            label: '${geometry.zones.length} Zonen',
                          ),
                          _FocusPill(
                            label: '${geometry.noGoZones.length} No-Go',
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                if (isSetupFlow || editorState.mode != MapEditorMode.view)
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
                if (isSetupFlow || editorState.mode != MapEditorMode.view)
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
                      if (editorState.activeObject == EditableMapObjectType.zone &&
                          editorState.activeZoneId != null) ...<Widget>[
                        const SizedBox(height: 12),
                        Align(
                          alignment: Alignment.centerLeft,
                          child: OutlinedButton.icon(
                            onPressed: () => _configureActiveZone(
                              context,
                              ref,
                              geometry,
                              editorState.activeZoneId!,
                            ),
                            icon: const Icon(Icons.tune_rounded),
                            label: const Text('Zone konfigurieren'),
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
                                      _setGeometry(ref, nextGeometry);
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
                                _setGeometry(ref, controller.deleteActiveObject(geometry, editorState));
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
    _setGeometry(ref, controller.deleteActiveObject(geometry, editorState));
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
    if (entryFlow == MapEditorEntryFlow.setup) {
      final hasPerimeter = geometry.perimeter.length >= 3;
      final hasDock = geometry.dock.isNotEmpty;
      final hasValidNoGo = geometry.noGoZones.every(
        (zone) => zone.isEmpty || zone.length >= 3,
      );
      final hasValidZones = geometry.zones.every(
        (zone) => zone.points.isEmpty || zone.points.length >= 3,
      );

      if (!hasPerimeter || !hasDock || !hasValidNoGo || !hasValidZones) {
        final missingParts = <String>[
          if (!hasPerimeter) 'Perimeter',
          if (!hasDock) 'Docking',
          if (!hasValidNoGo) 'gültige No-Go-Bereiche',
          if (!hasValidZones) 'vollständige Zonen',
        ];
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              'Speichern ist erst nach gültiger Basiskarte möglich. Es fehlt noch: ${missingParts.join(', ')}.',
            ),
          ),
        );
        return;
      }
    }

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
        if (entryFlow == MapEditorEntryFlow.setup) {
          context.go('/map/edit');
        }
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
        _setGeometry(ref, next);
        ref.read(mapEditorStateProvider.notifier).state = MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.noGo,
          activeNoGoIndex: next.noGoZones.length - 1,
        );
      case EditableMapObjectType.zone:
        final next = controller.addZone(geometry);
        final zoneId = next.zones.isEmpty ? null : next.zones.last.id;
        _setGeometry(ref, next);
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

  void _selectObjectForEditing(
    WidgetRef ref,
    MapEditorState nextState,
  ) {
    ref.read(mapEditorStateProvider.notifier).state = nextState.copyWith(
      clearSelectedPoint: true,
    );
  }

  void _enterEditMode(
    WidgetRef ref,
    MapGeometry geometry,
  ) {
    final nextState = geometry.perimeter.length >= 3
        ? const MapEditorState(
            mode: MapEditorMode.edit,
            activeObject: EditableMapObjectType.perimeter,
          )
        : geometry.zones.isNotEmpty
            ? MapEditorState(
                mode: MapEditorMode.edit,
                activeObject: EditableMapObjectType.zone,
                activeZoneId: geometry.zones.first.id,
              )
            : geometry.noGoZones.isNotEmpty
                ? const MapEditorState(
                    mode: MapEditorMode.edit,
                    activeObject: EditableMapObjectType.noGo,
                    activeNoGoIndex: 0,
                  )
                : geometry.dock.isNotEmpty
                    ? const MapEditorState(
                        mode: MapEditorMode.edit,
                        activeObject: EditableMapObjectType.dock,
                      )
                    : const MapEditorState(mode: MapEditorMode.view);
    ref.read(mapEditorStateProvider.notifier).state = nextState;
  }

  Future<void> _openAddMoreSheet(
    BuildContext context,
    WidgetRef ref,
    MapGeometry geometry,
  ) async {
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
                'Element hinzufügen',
                style: Theme.of(sheetContext).textTheme.titleLarge,
              ),
              const SizedBox(height: 8),
              Text(
                'Ergänze neue Kartenelemente in der Produktreihenfolge der Basiskarte.',
                style: Theme.of(sheetContext).textTheme.bodySmall,
              ),
              const SizedBox(height: 16),
              if (geometry.perimeter.isEmpty)
                _AddMoreTile(
                  icon: Icons.landscape_outlined,
                  title: 'Perimeter',
                  subtitle: 'Grundfläche der Karte aufnehmen',
                  onTap: () {
                    Navigator.of(sheetContext).pop();
                    _createObject(ref, geometry, EditableMapObjectType.perimeter);
                  },
                ),
              _AddMoreTile(
                icon: Icons.block_outlined,
                title: 'No-Go-Bereich',
                subtitle: geometry.perimeter.length >= 3
                    ? 'Ausschlussfläche ergänzen'
                    : 'Erst nach dem Perimeter verfügbar',
                onTap: geometry.perimeter.length >= 3
                    ? () {
                        Navigator.of(sheetContext).pop();
                        _createObject(ref, geometry, EditableMapObjectType.noGo);
                      }
                    : null,
              ),
              _AddMoreTile(
                icon: Icons.home_work_outlined,
                title: 'Docking',
                subtitle: geometry.perimeter.length >= 3
                    ? (geometry.dock.isEmpty
                        ? 'Dockingpunkt aufnehmen'
                        : 'Dockingpunkt neu setzen')
                    : 'Erst nach dem Perimeter verfügbar',
                onTap: geometry.perimeter.length >= 3
                    ? () {
                        Navigator.of(sheetContext).pop();
                        _createObject(ref, geometry, EditableMapObjectType.dock);
                      }
                    : null,
              ),
              _AddMoreTile(
                icon: Icons.crop_square_rounded,
                title: 'Zone',
                subtitle: canAddZone(geometry)
                    ? 'Optionale Mähzone zur vorhandenen Basiskarte hinzufügen'
                    : 'Erst nach gültiger Basiskarte mit Docking verfügbar',
                onTap: canAddZone(geometry)
                    ? () {
                        Navigator.of(sheetContext).pop();
                        _createObject(ref, geometry, EditableMapObjectType.zone);
                      }
                    : null,
              ),
            ],
          ),
        ),
      ),
    );
  }

  bool canAddZone(MapGeometry geometry) {
    final hasPerimeter = geometry.perimeter.length >= 3;
    final hasDock = geometry.dock.isNotEmpty;
    final hasValidNoGo = geometry.noGoZones.every((zone) => zone.isEmpty || zone.length >= 3);
    return hasPerimeter && hasDock && hasValidNoGo;
  }

  void _setSetupStage(
    WidgetRef ref,
    MapEditorState editorState,
    MapSetupStage stage,
  ) {
    ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(
      setupStage: stage,
      mode: MapEditorMode.view,
      activeObject: switch (stage) {
        MapSetupStage.perimeter => EditableMapObjectType.perimeter,
        MapSetupStage.noGo => EditableMapObjectType.noGo,
        MapSetupStage.dock => EditableMapObjectType.dock,
        MapSetupStage.validate || MapSetupStage.save => editorState.activeObject,
      },
      clearSelectedPoint: true,
      clearActiveZone: stage != MapSetupStage.save,
      clearActiveNoGo: stage != MapSetupStage.noGo,
    );
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
      _setGeometry(ref, next);
      ref.read(mapEditorStateProvider.notifier).state = editorState.copyWith(clearSelectedPoint: true);
      return;
    }

    final next = controller.addPoint(geometry, editorState, point);
    _setGeometry(ref, next);
  }

  Future<void> _configureActiveZone(
    BuildContext context,
    WidgetRef ref,
    MapGeometry geometry,
    String zoneId,
  ) async {
    final zone = geometry.zones.where((entry) => entry.id == zoneId).cast<ZoneGeometry?>().firstWhere(
          (entry) => entry != null,
          orElse: () => null,
        );
    if (zone == null) return;

    final nameController = TextEditingController(text: zone.name);
    var priority = zone.priority;
    var mowingDirection = zone.mowingDirection;
    var mowingProfile = zone.mowingProfile;

    final updatedZone = await showDialog<ZoneGeometry>(
      context: context,
      builder: (dialogContext) => StatefulBuilder(
        builder: (dialogContext, setDialogState) => AlertDialog(
          title: const Text('Zone konfigurieren'),
          content: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                TextField(
                  controller: nameController,
                  autofocus: true,
                  decoration: const InputDecoration(labelText: 'Zonenname'),
                ),
                const SizedBox(height: 12),
                DropdownButtonFormField<int>(
                  initialValue: priority,
                  decoration: const InputDecoration(labelText: 'Priorität'),
                  items: const <DropdownMenuItem<int>>[
                    DropdownMenuItem(value: 1, child: Text('1 - Normal')),
                    DropdownMenuItem(value: 2, child: Text('2 - Hoch')),
                    DropdownMenuItem(value: 3, child: Text('3 - Sehr hoch')),
                  ],
                  onChanged: (value) {
                    if (value == null) return;
                    setDialogState(() => priority = value);
                  },
                ),
                const SizedBox(height: 12),
                DropdownButtonFormField<String>(
                  initialValue: mowingDirection,
                  decoration: const InputDecoration(labelText: 'Mährichtung'),
                  items: const <DropdownMenuItem<String>>[
                    DropdownMenuItem(value: 'Automatisch', child: Text('Automatisch')),
                    DropdownMenuItem(value: 'Nord-Süd', child: Text('Nord-Süd')),
                    DropdownMenuItem(value: 'Ost-West', child: Text('Ost-West')),
                    DropdownMenuItem(value: 'Diagonal', child: Text('Diagonal')),
                  ],
                  onChanged: (value) {
                    if (value == null) return;
                    setDialogState(() => mowingDirection = value);
                  },
                ),
                const SizedBox(height: 12),
                DropdownButtonFormField<String>(
                  initialValue: mowingProfile,
                  decoration: const InputDecoration(labelText: 'Mähprofil'),
                  items: const <DropdownMenuItem<String>>[
                    DropdownMenuItem(value: 'Standard', child: Text('Standard')),
                    DropdownMenuItem(value: 'Schnell', child: Text('Schnell')),
                    DropdownMenuItem(value: 'Sorgfältig', child: Text('Sorgfältig')),
                  ],
                  onChanged: (value) {
                    if (value == null) return;
                    setDialogState(() => mowingProfile = value);
                  },
                ),
              ],
            ),
          ),
          actions: <Widget>[
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('Abbrechen'),
            ),
            FilledButton(
              onPressed: () {
                final nextName = nameController.text.trim();
                if (nextName.isEmpty) return;
                Navigator.of(dialogContext).pop(
                  zone.copyWith(
                    name: nextName,
                    priority: priority,
                    mowingDirection: mowingDirection,
                    mowingProfile: mowingProfile,
                  ),
                );
              },
              child: const Text('Speichern'),
            ),
          ],
        ),
      ),
    );
    if (updatedZone == null) return;

    final nextZones = geometry.zones
        .map((entry) => entry.id == zoneId ? updatedZone : entry)
        .toList(growable: false);
    _setGeometry(ref, geometry.copyWith(zones: nextZones));
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

  bool _canOpenSetupStage(
    MapSetupStage stage, {
    required bool hasPerimeter,
    required bool hasDock,
  }) {
    switch (stage) {
      case MapSetupStage.perimeter:
        return true;
      case MapSetupStage.noGo:
        return hasPerimeter;
      case MapSetupStage.dock:
        return hasPerimeter;
      case MapSetupStage.validate:
        return hasPerimeter && hasDock;
      case MapSetupStage.save:
        return hasPerimeter && hasDock;
    }
  }

  bool _isSetupStageDone(
    MapSetupStage stage, {
    required bool hasPerimeter,
    required bool hasDock,
    required bool hasValidNoGo,
    required bool setupReady,
  }) {
    switch (stage) {
      case MapSetupStage.perimeter:
        return hasPerimeter;
      case MapSetupStage.noGo:
        return hasValidNoGo;
      case MapSetupStage.dock:
        return hasDock;
      case MapSetupStage.validate:
        return setupReady;
      case MapSetupStage.save:
        return false;
    }
  }

  String _setupStageLabel(MapSetupStage stage) {
    switch (stage) {
      case MapSetupStage.perimeter:
        return '1 Perimeter';
      case MapSetupStage.noGo:
        return '2 No-Go';
      case MapSetupStage.dock:
        return '3 Dock';
      case MapSetupStage.validate:
        return '4 Validierung';
      case MapSetupStage.save:
        return '5 Speichern';
    }
  }

  String _setupStageDescription(MapSetupStage stage) {
    switch (stage) {
      case MapSetupStage.perimeter:
        return 'Definiere zuerst die Außenkante des Gartens. Ohne Perimeter geht der Rest nicht weiter.';
      case MapSetupStage.noGo:
        return 'Ergänze optional Ausschlussflächen für Beete, Inseln oder sensible Bereiche.';
      case MapSetupStage.dock:
        return 'Setze die Dock-Position erst nach der Grundfläche, damit die Karte fachlich vollständig ist.';
      case MapSetupStage.validate:
        return 'Prüfe jetzt, ob die Karte in sich stimmig ist. Erst danach sollte sie gespeichert werden.';
      case MapSetupStage.save:
        return 'Speichere die Basiskarte. Zonen und spätere Korrekturen folgen anschließend im Bearbeitungsflow.';
    }
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
                      const Text('Schließen'),
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

class _FlowStatusRow extends StatelessWidget {
  const _FlowStatusRow({
    required this.done,
    required this.title,
    required this.subtitle,
  });

  final bool done;
  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Icon(
          done ? Icons.check_circle_rounded : Icons.radio_button_unchecked_rounded,
          size: 18,
          color: done ? const Color(0xFF4ADE80) : const Color(0xFFF59E0B),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                title,
                style: Theme.of(context).textTheme.titleSmall,
              ),
              const SizedBox(height: 2),
              Text(
                subtitle,
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _EditShortcutChip extends StatelessWidget {
  const _EditShortcutChip({
    required this.label,
    required this.enabled,
    required this.onTap,
  });

  final String label;
  final bool enabled;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return ActionChip(
      label: Text(label),
      onPressed: enabled ? onTap : null,
      avatar: const Icon(Icons.edit_rounded, size: 16),
    );
  }
}

class _BaseToolChip extends StatelessWidget {
  const _BaseToolChip({
    required this.label,
    required this.subtitle,
    required this.selected,
    required this.enabled,
    required this.done,
    required this.onTap,
    this.optional = false,
  });

  final String label;
  final String subtitle;
  final bool selected;
  final bool enabled;
  final bool done;
  final VoidCallback onTap;
  final bool optional;

  @override
  Widget build(BuildContext context) {
    final borderColor = selected
        ? const Color(0xFF2563EB)
        : done
            ? const Color(0xFF16A34A)
            : const Color(0xFFCBD5E1);

    return InkWell(
      onTap: enabled ? onTap : null,
      borderRadius: BorderRadius.circular(16),
      child: Container(
        width: 148,
        padding: const EdgeInsets.fromLTRB(12, 12, 12, 12),
        decoration: BoxDecoration(
          color: selected ? const Color(0xFFEFF6FF) : const Color(0xFFF8FAFC),
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: borderColor),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                Icon(
                  done ? Icons.check_circle_rounded : Icons.radio_button_unchecked_rounded,
                  size: 16,
                  color: done ? const Color(0xFF16A34A) : const Color(0xFF64748B),
                ),
                const SizedBox(width: 6),
                if (optional)
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                    decoration: BoxDecoration(
                      color: const Color(0xFFE2E8F0),
                      borderRadius: BorderRadius.circular(999),
                    ),
                    child: Text(
                      'optional',
                      style: Theme.of(context).textTheme.labelSmall?.copyWith(
                            color: const Color(0xFF334155),
                            fontWeight: FontWeight.w700,
                          ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 10),
            Text(
              label,
              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w700,
                    color: enabled ? const Color(0xFF0F172A) : const Color(0xFF94A3B8),
                  ),
            ),
            const SizedBox(height: 4),
            Text(
              subtitle,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: enabled ? const Color(0xFF475569) : const Color(0xFF94A3B8),
                  ),
            ),
          ],
        ),
      ),
    );
  }
}

class _SetupStageCard extends StatelessWidget {
  const _SetupStageCard({
    required this.stage,
    required this.setupReady,
    required this.hasPerimeter,
    required this.hasDock,
    required this.hasValidNoGo,
    required this.onPrimaryAction,
    required this.onSecondaryAction,
  });

  final MapSetupStage stage;
  final bool setupReady;
  final bool hasPerimeter;
  final bool hasDock;
  final bool hasValidNoGo;
  final VoidCallback onPrimaryAction;
  final VoidCallback onSecondaryAction;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final primaryLabel = switch (stage) {
      MapSetupStage.perimeter => 'Perimeter aufzeichnen',
      MapSetupStage.noGo => 'No-Go aufnehmen',
      MapSetupStage.dock => 'Dock setzen',
      MapSetupStage.validate => 'Validierung abschließen',
      MapSetupStage.save => 'Karte speichern',
    };
    final secondaryLabel = switch (stage) {
      MapSetupStage.perimeter => null,
      MapSetupStage.noGo => 'Ohne No-Go weiter',
      MapSetupStage.dock => 'Dock später setzen',
      MapSetupStage.validate => 'Zurück zu No-Go',
      MapSetupStage.save => 'Zur Bearbeitung',
    };
    final primaryEnabled = switch (stage) {
      MapSetupStage.perimeter => true,
      MapSetupStage.noGo => hasPerimeter,
      MapSetupStage.dock => hasPerimeter,
      MapSetupStage.validate => setupReady,
      MapSetupStage.save => setupReady,
    };

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: const Color(0xFF0B1424),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: theme.dividerColor),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            switch (stage) {
              MapSetupStage.perimeter => 'Grundfläche aufnehmen',
              MapSetupStage.noGo => 'Ausschlussbereiche ergänzen',
              MapSetupStage.dock => 'Dockingpunkt festlegen',
              MapSetupStage.validate => 'Karte fachlich prüfen',
              MapSetupStage.save => 'Basiskarte abschließen',
            },
            style: theme.textTheme.titleSmall,
          ),
          const SizedBox(height: 6),
          Text(
            switch (stage) {
              MapSetupStage.perimeter => 'Setze mindestens drei Punkte für die Außenkante.',
              MapSetupStage.noGo => hasValidNoGo
                  ? 'Vorhandene No-Go-Bereiche sind gültig. Du kannst weitergehen oder weitere Flächen aufnehmen.'
                  : 'Beende angefangene No-Go-Bereiche mit mindestens drei Punkten.',
              MapSetupStage.dock => hasDock
                  ? 'Dock ist bereits gesetzt. Du kannst direkt validieren.'
                  : 'Lege einen Dock-Punkt auf der Karte fest.',
              MapSetupStage.validate => 'Alle Pflichtpunkte müssen vorhanden sein, bevor die Karte gespeichert wird.',
              MapSetupStage.save => 'Nach dem Speichern wechselst du in die Kartenbearbeitung für Zonen und spätere Anpassungen.',
            },
            style: theme.textTheme.bodySmall,
          ),
          const SizedBox(height: 12),
          Row(
            children: <Widget>[
              Expanded(
                child: FilledButton.icon(
                  onPressed: primaryEnabled ? onPrimaryAction : null,
                  icon: const Icon(Icons.arrow_forward_rounded),
                  label: Text(primaryLabel),
                ),
              ),
            ],
          ),
          if (secondaryLabel != null) ...<Widget>[
            const SizedBox(height: 8),
            Row(
              children: <Widget>[
                Expanded(
                  child: OutlinedButton(
                    onPressed: onSecondaryAction,
                    child: Text(secondaryLabel),
                  ),
                ),
              ],
            ),
          ],
        ],
      ),
    );
  }
}

class _ValidationItem {
  const _ValidationItem({
    required this.done,
    required this.label,
  });

  final bool done;
  final String label;
}

class _ValidationList extends StatelessWidget {
  const _ValidationList({required this.items});

  final List<_ValidationItem> items;

  @override
  Widget build(BuildContext context) {
    return Column(
      children: items
          .map(
            (item) => Padding(
              padding: const EdgeInsets.only(bottom: 8),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Icon(
                    item.done ? Icons.check_circle_rounded : Icons.error_outline_rounded,
                    size: 18,
                    color: item.done ? const Color(0xFF4ADE80) : const Color(0xFFF59E0B),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: Text(
                      item.label,
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ),
          )
          .toList(growable: false),
    );
  }
}

class _AddMoreTile extends StatelessWidget {
  const _AddMoreTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    return ListTile(
      enabled: onTap != null,
      contentPadding: EdgeInsets.zero,
      leading: Icon(icon),
      title: Text(title),
      subtitle: Text(subtitle),
      trailing: const Icon(Icons.chevron_right_rounded),
      onTap: onTap,
    );
  }
}

class _MapOverlayTitleCard extends StatelessWidget {
  const _MapOverlayTitleCard({
    required this.title,
    required this.subtitle,
  });

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: const Color(0xC20F172A),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: const Color(0x33FFFFFF)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            title,
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
          ),
          const SizedBox(height: 2),
          Text(
            subtitle,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: const Color(0xFFBFDBFE),
                ),
          ),
        ],
      ),
    );
  }
}

class _MapOverlayIconButton extends StatelessWidget {
  const _MapOverlayIconButton({
    required this.icon,
    required this.tooltip,
    this.onPressed,
  });

  final IconData icon;
  final String tooltip;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tooltip,
      child: Material(
        color: const Color(0xC20F172A),
        shape: const CircleBorder(),
        child: IconButton(
          onPressed: onPressed,
          icon: Icon(icon, color: Colors.white),
        ),
      ),
    );
  }
}

class _FloatingActionPill extends StatelessWidget {
  const _FloatingActionPill({
    required this.icon,
    required this.label,
    required this.onTap,
  });

  final IconData icon;
  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xD90F172A),
      borderRadius: BorderRadius.circular(999),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(999),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              Icon(icon, size: 18, color: const Color(0xFF93C5FD)),
              const SizedBox(width: 8),
              Text(
                label,
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _FocusPill extends StatelessWidget {
  const _FocusPill({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: const Color(0xFF0F1829),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: const Color(0xFF1E3A5F)),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.bodySmall,
      ),
    );
  }
}
