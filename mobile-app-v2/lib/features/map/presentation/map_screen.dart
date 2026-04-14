import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../domain/map/map_geometry.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../shared/widgets/robot_map_view.dart';
import '../domain/map_editor_state.dart';

class MapScreen extends StatelessWidget {
  const MapScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final editorState = controller.mapEditorState;
    final activePoints = controller.activeMapPoints;
    final segmentPoints = _segmentTargets(activePoints);
    final bottomInset = MediaQuery.paddingOf(context).bottom;
    final status =
        controller.connectionStatus.connectionState ==
            ConnectionStateKind.connected
        ? controller.connectionStatus
        : null;

    return Scaffold(
      backgroundColor: const Color(0xFFE8E8EC),
      body: Stack(
        children: <Widget>[
          Positioned.fill(
            child: RobotMapView(
              map: controller.mapGeometry,
              status: status,
              interactive: true,
              onMapTap: (point) => _handleMapTap(context, controller, point),
              activePoints: activePoints,
              segmentPoints: editorState.mode == MapEditorMode.edit
                  ? segmentPoints
                  : const <EditableSegmentTarget>[],
              onPointTap: controller.selectMapPoint,
              onPointDragStart: editorState.mode == MapEditorMode.edit
                  ? controller.startMapPointDrag
                  : null,
              onPointDragUpdate: editorState.mode == MapEditorMode.edit
                  ? controller.dragMapPoint
                  : null,
              onPointDragEnd: editorState.mode == MapEditorMode.edit
                  ? controller.finishMapPointDrag
                  : null,
              onSegmentTap: (insertIndex) {
                final target = segmentPoints.firstWhere(
                  (segment) => segment.insertIndex == insertIndex,
                );
                controller.insertMapPoint(insertIndex, target.point);
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
              previewRoutePoints: controller.previewRoutePoints,
              showCenterButton: editorState.mode == MapEditorMode.view,
              onZoneTap: controller.setActiveZone,
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
                      Color(0x7A0F1829),
                      Color(0x000F1829),
                      Color(0x000F1829),
                      Color(0xB30F1829),
                    ],
                    stops: <double>[0, 0.18, 0.6, 1],
                  ),
                ),
              ),
            ),
          ),
          SafeArea(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(12, 10, 12, 0),
              child: Column(
                children: <Widget>[
                  Row(
                    children: <Widget>[
                      _RoundMapButton(
                        icon: Icons.arrow_back_rounded,
                        onTap: () {
                          if (context.canPop()) {
                            context.pop();
                          } else {
                            context.go('/dashboard');
                          }
                        },
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: _TitlePill(
                          title: 'Karteneditor',
                          subtitle: _activeObjectLabel(controller),
                        ),
                      ),
                      const SizedBox(width: 10),
                      _RoundMapButton(
                        icon: Icons.undo_rounded,
                        onTap: controller.canUndoMapEdit
                            ? controller.undoMapEdit
                            : null,
                      ),
                      const SizedBox(width: 8),
                      _RoundMapButton(
                        icon: Icons.redo_rounded,
                        onTap: controller.canRedoMapEdit
                            ? controller.redoMapEdit
                            : null,
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: <Widget>[
                      _StatusPill(
                        icon: Icons.place_rounded,
                        label: '${activePoints.length} Punkte',
                      ),
                      const SizedBox(width: 8),
                      _StatusPill(
                        icon: Icons.satellite_alt_rounded,
                        label:
                            status != null &&
                                status.gpsLat != null &&
                                status.gpsLon != null
                            ? 'GPS bereit'
                            : 'GPS fehlt',
                      ),
                      const Spacer(),
                      _RoundMapButton(
                        icon: Icons.gps_fixed_rounded,
                        onTap: () => _openGpsSheet(context, controller),
                      ),
                      const SizedBox(width: 8),
                      _RoundMapButton(
                        icon: editorState.selectedPointIndex != null
                            ? Icons.remove_circle_outline_rounded
                            : Icons.delete_outline_rounded,
                        onTap: editorState.selectedPointIndex != null
                            ? controller.deleteSelectedMapPoint
                            : (activePoints.isNotEmpty ||
                                  controller.mapGeometry.zones.isNotEmpty ||
                                  controller.mapGeometry.noGoZones.isNotEmpty ||
                                  controller.mapGeometry.dock.isNotEmpty)
                            ? controller.deleteActiveMapObject
                            : null,
                      ),
                    ],
                  ),
                  const Spacer(),
                ],
              ),
            ),
          ),
          Positioned(
            top: MediaQuery.sizeOf(context).height * 0.24,
            right: 12,
            child: Column(
              children: <Widget>[
                _ActionTile(
                  icon: Icons.add_rounded,
                  title: 'Objekt',
                  subtitle: 'hinzufügen',
                  accentColor: const Color(0xFF2F7D4A),
                  foregroundColor: Colors.white,
                  onTap: () => context.push('/map/add-object'),
                ),
                const SizedBox(height: 12),
                _ActionTile(
                  icon: editorState.mode == MapEditorMode.record
                      ? Icons.check_rounded
                      : Icons.edit_location_alt_rounded,
                  title: editorState.mode == MapEditorMode.record
                      ? 'Fertig'
                      : 'Aufzeichnen',
                  subtitle: editorState.mode == MapEditorMode.record
                      ? 'abschließen'
                      : 'GPS oder Tap',
                  accentColor: const Color(0xFF3A3A3C),
                  foregroundColor: Colors.white,
                  onTap: () {
                    if (editorState.mode == MapEditorMode.record) {
                      controller.finishMapRecording();
                    } else {
                      controller.setMapEditorMode(MapEditorMode.record);
                    }
                  },
                ),
              ],
            ),
          ),
          Positioned(
            left: 12,
            right: 12,
            bottom: 24 + bottomInset,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                _SetupStageCard(controller: controller),
                const SizedBox(height: 12),
                _ModePanel(controller: controller, bottomInset: bottomInset),
                if (editorState.mode == MapEditorMode.record) ...<Widget>[
                  const SizedBox(height: 12),
                  _RecordPanel(
                    status: controller.connectionStatus,
                    activePoints: activePoints,
                    activeObject: editorState.activeObject,
                    onSavePoint: () {
                      final gpsLat = controller.connectionStatus.gpsLat;
                      final gpsLon = controller.connectionStatus.gpsLon;
                      if (gpsLat == null || gpsLon == null) {
                        return;
                      }
                      HapticFeedback.selectionClick();
                      controller.addMapPoint(MapPoint(x: gpsLon, y: gpsLat));
                    },
                    onClose: controller.canCloseActiveMapObject()
                        ? controller.finishMapRecording
                        : null,
                    onCancel: () =>
                        _confirmCancelRecording(context, controller),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }

  void _handleMapTap(
    BuildContext context,
    AppController controller,
    MapPoint point,
  ) {
    final editorState = controller.mapEditorState;
    if (editorState.mode == MapEditorMode.record) {
      controller.addMapPoint(point);
      return;
    }
    if (editorState.mode == MapEditorMode.edit &&
        editorState.selectedPointIndex != null) {
      controller.moveSelectedMapPoint(point);
      return;
    }
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text(
          'Wähle zuerst Aufzeichnen oder Bearbeiten, um Punkte zu ändern.',
        ),
      ),
    );
  }

  Future<void> _openGpsSheet(BuildContext context, AppController controller) {
    return showModalBottomSheet<void>(
      context: context,
      builder: (sheetContext) {
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  'GPS-Referenz',
                  style: Theme.of(sheetContext).textTheme.titleMedium,
                ),
                const SizedBox(height: 12),
                _GpsRow(
                  label: 'Fix-Qualität',
                  value:
                      controller.connectionStatus.rtkState ??
                      controller.statusLabel,
                ),
                _GpsRow(
                  label: 'Latitude',
                  value:
                      controller.connectionStatus.gpsLat?.toStringAsFixed(7) ??
                      '-',
                ),
                _GpsRow(
                  label: 'Longitude',
                  value:
                      controller.connectionStatus.gpsLon?.toStringAsFixed(7) ??
                      '-',
                ),
                _GpsRow(
                  label: 'Kartenfläche',
                  value: '${controller.mapAreaSquareMeters} m²',
                ),
                _GpsRow(
                  label: 'Perimeter-Punkte',
                  value: '${controller.perimeterPointCount}',
                ),
                _GpsRow(label: 'Kanäle', value: '${controller.channelCount}'),
                _GpsRow(label: 'No-Go-Zonen', value: '${controller.noGoCount}'),
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _confirmCancelRecording(
    BuildContext context,
    AppController controller,
  ) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: const Text('Aufzeichnung abbrechen'),
        content: const Text(
          'Alle Änderungen seit Start der Aufzeichnung werden verworfen.',
        ),
        actions: <Widget>[
          TextButton(
            onPressed: () => Navigator.pop(dialogContext, false),
            child: const Text('Weiter aufzeichnen'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(dialogContext, true),
            child: const Text('Verwerfen'),
          ),
        ],
      ),
    );
    if (confirmed == true) {
      controller.cancelMapRecording();
    }
  }

  String _activeObjectLabel(AppController controller) {
    final state = controller.mapEditorState;
    return switch (state.activeObject) {
      EditableMapObjectType.perimeter => 'Perimeter',
      EditableMapObjectType.noGo => 'No-Go',
      EditableMapObjectType.zone =>
        controller.mapGeometry.zones
            .firstWhere(
              (zone) => zone.id == state.activeZoneId,
              orElse: () => const ZoneGeometry(
                id: '',
                name: 'Zone',
                points: <MapPoint>[],
              ),
            )
            .name,
      EditableMapObjectType.dock => 'Dock',
    };
  }
}

List<EditableSegmentTarget> _segmentTargets(List<MapPoint> activePoints) {
  if (activePoints.length < 2) {
    return const <EditableSegmentTarget>[];
  }
  final targets = <EditableSegmentTarget>[];
  for (var index = 0; index < activePoints.length - 1; index += 1) {
    final current = activePoints[index];
    final next = activePoints[index + 1];
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
  return targets;
}

Future<void> _openZoneConfigSheet(
  BuildContext context,
  AppController controller,
  ZoneGeometry zone,
) async {
  final nameController = TextEditingController(text: zone.name);
  var priority = zone.priority;
  var mowingDirection = zone.mowingDirection;
  var mowingProfile = zone.mowingProfile;

  await showModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    builder: (sheetContext) {
      return StatefulBuilder(
        builder: (context, setState) {
          return Padding(
            padding: EdgeInsets.fromLTRB(
              20,
              20,
              20,
              20 + MediaQuery.viewInsetsOf(context).bottom,
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  'Zone konfigurieren',
                  style: Theme.of(
                    context,
                  ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.w800),
                ),
                const SizedBox(height: 16),
                TextField(
                  controller: nameController,
                  decoration: const InputDecoration(
                    labelText: 'Name',
                    border: OutlineInputBorder(),
                  ),
                ),
                const SizedBox(height: 12),
                DropdownButtonFormField<int>(
                  initialValue: priority,
                  decoration: const InputDecoration(
                    labelText: 'Priorität',
                    border: OutlineInputBorder(),
                  ),
                  items: List<DropdownMenuItem<int>>.generate(
                    5,
                    (index) => DropdownMenuItem<int>(
                      value: index + 1,
                      child: Text('Priorität ${index + 1}'),
                    ),
                  ),
                  onChanged: (value) {
                    if (value == null) {
                      return;
                    }
                    setState(() => priority = value);
                  },
                ),
                const SizedBox(height: 12),
                DropdownButtonFormField<String>(
                  initialValue: mowingDirection,
                  decoration: const InputDecoration(
                    labelText: 'Mährichtung',
                    border: OutlineInputBorder(),
                  ),
                  items: const <DropdownMenuItem<String>>[
                    DropdownMenuItem(
                      value: 'Automatisch',
                      child: Text('Automatisch'),
                    ),
                    DropdownMenuItem(
                      value: 'Nord-Sued',
                      child: Text('Nord-Süd'),
                    ),
                    DropdownMenuItem(
                      value: 'Ost-West',
                      child: Text('Ost-West'),
                    ),
                    DropdownMenuItem(
                      value: 'Diagonal',
                      child: Text('Diagonal'),
                    ),
                  ],
                  onChanged: (value) {
                    if (value == null) {
                      return;
                    }
                    setState(() => mowingDirection = value);
                  },
                ),
                const SizedBox(height: 12),
                DropdownButtonFormField<String>(
                  initialValue: mowingProfile,
                  decoration: const InputDecoration(
                    labelText: 'Mähprofil',
                    border: OutlineInputBorder(),
                  ),
                  items: const <DropdownMenuItem<String>>[
                    DropdownMenuItem(
                      value: 'Standard',
                      child: Text('Standard'),
                    ),
                    DropdownMenuItem(value: 'Schnell', child: Text('Schnell')),
                    DropdownMenuItem(
                      value: 'Sorgfaeltig',
                      child: Text('Sorgfältig'),
                    ),
                  ],
                  onChanged: (value) {
                    if (value == null) {
                      return;
                    }
                    setState(() => mowingProfile = value);
                  },
                ),
                const SizedBox(height: 16),
                SizedBox(
                  width: double.infinity,
                  child: FilledButton(
                    onPressed: () {
                      controller.updateZoneConfiguration(
                        zone.id,
                        name: nameController.text.trim().isEmpty
                            ? zone.name
                            : nameController.text.trim(),
                        priority: priority,
                        mowingDirection: mowingDirection,
                        mowingProfile: mowingProfile,
                      );
                      Navigator.of(sheetContext).pop();
                    },
                    child: const Text('Zone speichern'),
                  ),
                ),
              ],
            ),
          );
        },
      );
    },
  );
}

class _SetupStageCard extends StatelessWidget {
  const _SetupStageCard({required this.controller});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final stage = controller.mapEditorState.setupStage;

    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF2FFFFFF),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              'Setup-Flow',
              style: Theme.of(
                context,
              ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 6),
            Text(
              _stageDescription(stage),
              style: Theme.of(
                context,
              ).textTheme.bodyMedium?.copyWith(color: const Color(0xFF667267)),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: MapSetupStage.values
                  .map(
                    (candidate) => ChoiceChip(
                      label: Text(_stageLabel(candidate)),
                      selected: candidate == stage,
                      onSelected: (_) => controller.setMapSetupStage(candidate),
                    ),
                  )
                  .toList(growable: false),
            ),
            const SizedBox(height: 12),
            if (stage == MapSetupStage.validate ||
                stage == MapSetupStage.save) ...<Widget>[
              for (final issue in controller.mapValidationIssues)
                Padding(
                  padding: const EdgeInsets.only(bottom: 8),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Icon(
                        issue.blocking
                            ? Icons.error_outline_rounded
                            : Icons.info_outline_rounded,
                        size: 18,
                        color: issue.blocking
                            ? const Color(0xFFB91C1C)
                            : const Color(0xFF1D4ED8),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          '${issue.label}: ${issue.message}',
                          style: Theme.of(context).textTheme.bodySmall
                              ?.copyWith(
                                color: issue.blocking
                                    ? const Color(0xFF7F1D1D)
                                    : const Color(0xFF1E3A8A),
                                fontWeight: issue.blocking
                                    ? FontWeight.w700
                                    : FontWeight.w500,
                              ),
                        ),
                      ),
                    ],
                  ),
                ),
              if (!controller.hasBlockingMapValidationIssues)
                Text(
                  'Basiskarte ist gültig. Du kannst speichern und bei Bedarf direkt eine Planner-Vorschau laden.',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: const Color(0xFF166534),
                    fontWeight: FontWeight.w700,
                  ),
                ),
              if (controller.isLoadingPreview) ...<Widget>[
                const SizedBox(height: 10),
                const LinearProgressIndicator(minHeight: 3),
              ],
            ],
            const SizedBox(height: 12),
            Row(
              children: <Widget>[
                Expanded(
                  child: OutlinedButton(
                    onPressed: stage == MapSetupStage.perimeter
                        ? null
                        : controller.retreatMapSetupStage,
                    child: const Text('Zurück'),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: FilledButton(
                    onPressed: stage == MapSetupStage.save
                        ? null
                        : controller.advanceMapSetupStage,
                    child: Text(
                      stage == MapSetupStage.validate
                          ? 'Prüfung fertig'
                          : 'Weiter',
                    ),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  String _stageLabel(MapSetupStage stage) {
    return switch (stage) {
      MapSetupStage.perimeter => 'Perimeter',
      MapSetupStage.noGo => 'No-Go',
      MapSetupStage.dock => 'Dock',
      MapSetupStage.validate => 'Prüfen',
      MapSetupStage.save => 'Speichern',
    };
  }

  String _stageDescription(MapSetupStage stage) {
    return switch (stage) {
      MapSetupStage.perimeter =>
        'Ziehe zuerst den Perimeter auf oder ergänze fehlende Randpunkte.',
      MapSetupStage.noGo =>
        'Lege optionale Sperrzonen an. Diesen Schritt kannst du auch überspringen.',
      MapSetupStage.dock =>
        'Setze die Dock-Position mit mindestens zwei Punkten.',
      MapSetupStage.validate =>
        'Prüfe die Basiskarte, bevor du speicherst oder eine Vorschau lädst.',
      MapSetupStage.save =>
        'Basiskarte steht. Jetzt kannst du Zonen verfeinern, speichern und die Planner-Vorschau laden.',
    };
  }
}

class _ModePanel extends StatelessWidget {
  const _ModePanel({required this.controller, required this.bottomInset});

  final AppController controller;
  final double bottomInset;

  @override
  Widget build(BuildContext context) {
    final editorState = controller.mapEditorState;
    final activeZone = editorState.activeZoneId == null
        ? null
        : controller.mapGeometry.zones.firstWhere(
            (zone) => zone.id == editorState.activeZoneId,
            orElse: () =>
                const ZoneGeometry(id: '', name: '', points: <MapPoint>[]),
          );

    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF2FFFFFF),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Padding(
        padding: EdgeInsets.fromLTRB(16, 16, 16, 16 + bottomInset * 0.15),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Row(
              children: <Widget>[
                Expanded(
                  child: Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: <Widget>[
                      _ModeChip(
                        label: 'Ansicht',
                        selected: editorState.mode == MapEditorMode.view,
                        onTap: () =>
                            controller.setMapEditorMode(MapEditorMode.view),
                      ),
                      _ModeChip(
                        label: 'Bearbeiten',
                        selected: editorState.mode == MapEditorMode.edit,
                        onTap: () =>
                            controller.setMapEditorMode(MapEditorMode.edit),
                      ),
                      _ModeChip(
                        label: 'Aufzeichnen',
                        selected: editorState.mode == MapEditorMode.record,
                        onTap: () =>
                            controller.setMapEditorMode(MapEditorMode.record),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 12),
                SizedBox(
                  width: 136,
                  child: FilledButton(
                    onPressed: () async {
                      final messenger = ScaffoldMessenger.of(context);
                      final result = await controller.saveMapEdits();
                      messenger.showSnackBar(
                        SnackBar(
                          content: Text(
                            result ??
                                (controller.connectedRobot == null
                                    ? 'Karte lokal gespeichert.'
                                    : 'Karte auf Alfred gespeichert.'),
                          ),
                        ),
                      );
                    },
                    style: FilledButton.styleFrom(
                      backgroundColor: const Color(0xFF2F7D4A),
                      foregroundColor: Colors.white,
                    ),
                    child: const Text('Speichern'),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Row(
              children: <Widget>[
                if (editorState.activeObject == EditableMapObjectType.zone &&
                    activeZone != null &&
                    activeZone.id.isNotEmpty) ...<Widget>[
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () =>
                          _openZoneConfigSheet(context, controller, activeZone),
                      icon: const Icon(Icons.tune_rounded),
                      label: const Text('Zone konfigurieren'),
                    ),
                  ),
                  const SizedBox(width: 10),
                ],
                Expanded(
                  child: FilledButton.tonalIcon(
                    onPressed: controller.isLoadingPreview
                        ? null
                        : () async {
                            final messenger = ScaffoldMessenger.of(context);
                            final result = await controller
                                .loadPlannerPreview();
                            messenger.showSnackBar(
                              SnackBar(
                                content: Text(
                                  result ?? 'Planner-Vorschau geladen.',
                                ),
                              ),
                            );
                          },
                    icon: Icon(
                      controller.isLoadingPreview
                          ? Icons.sync_rounded
                          : Icons.alt_route_rounded,
                    ),
                    label: Text(
                      controller.isLoadingPreview
                          ? 'Lädt...'
                          : 'Planner-Vorschau',
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: <Widget>[
                _ObjectChip(
                  label: 'Perimeter',
                  selected:
                      editorState.activeObject ==
                      EditableMapObjectType.perimeter,
                  onTap: () => controller.setActiveMapObject(
                    EditableMapObjectType.perimeter,
                    mode: MapEditorMode.edit,
                  ),
                ),
                _ObjectChip(
                  label: 'Dock',
                  selected:
                      editorState.activeObject == EditableMapObjectType.dock,
                  onTap: () => controller.setActiveMapObject(
                    EditableMapObjectType.dock,
                    mode: MapEditorMode.edit,
                  ),
                ),
                for (
                  var index = 0;
                  index < controller.mapGeometry.noGoZones.length;
                  index += 1
                )
                  _ObjectChip(
                    label: 'No-Go ${index + 1}',
                    selected:
                        editorState.activeObject ==
                            EditableMapObjectType.noGo &&
                        editorState.activeNoGoIndex == index,
                    onTap: () => controller.setActiveMapObject(
                      EditableMapObjectType.noGo,
                      noGoIndex: index,
                      mode: MapEditorMode.edit,
                    ),
                  ),
                for (final zone in controller.mapGeometry.zones)
                  _ObjectChip(
                    label: zone.name,
                    selected:
                        editorState.activeObject ==
                            EditableMapObjectType.zone &&
                        editorState.activeZoneId == zone.id,
                    onTap: () => controller.setActiveMapObject(
                      EditableMapObjectType.zone,
                      zoneId: zone.id,
                      mode: MapEditorMode.edit,
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 14),
            _MapSizePill(
              currentSquareMeters: controller.mapAreaSquareMeters,
              maxSquareMeters: 5000,
            ),
            if (editorState.mode == MapEditorMode.edit &&
                editorState.selectedPointIndex != null) ...<Widget>[
              const SizedBox(height: 12),
              Text(
                'Punkt ${editorState.selectedPointIndex! + 1} ausgewählt. Ziehe ihn direkt oder tippe auf die Karte, um ihn zu verschieben.',
                style: Theme.of(
                  context,
                ).textTheme.bodySmall?.copyWith(color: const Color(0xFF667267)),
              ),
            ],
            if (controller.previewError != null) ...<Widget>[
              const SizedBox(height: 12),
              Text(
                controller.previewError!,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: const Color(0xFF9F1239),
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _RecordPanel extends StatelessWidget {
  const _RecordPanel({
    required this.status,
    required this.activePoints,
    required this.activeObject,
    required this.onSavePoint,
    required this.onClose,
    required this.onCancel,
  });

  final RobotStatus status;
  final List<MapPoint> activePoints;
  final EditableMapObjectType activeObject;
  final VoidCallback onSavePoint;
  final VoidCallback? onClose;
  final VoidCallback onCancel;

  @override
  Widget build(BuildContext context) {
    final hasGps =
        status.connectionState == ConnectionStateKind.connected &&
        status.gpsLat != null &&
        status.gpsLon != null;
    final lastPoint = activePoints.isNotEmpty ? activePoints.last : null;
    final minimumPoints = activeObject == EditableMapObjectType.dock ? 2 : 3;

    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF0F1829),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: const Color(0xFF27364C)),
      ),
      padding: const EdgeInsets.fromLTRB(16, 14, 16, 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: <Widget>[
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: <Widget>[
              Text(
                '${activePoints.length} Punkt${activePoints.length == 1 ? '' : 'e'}',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: const Color(0xFF93C5FD),
                  fontWeight: FontWeight.w700,
                ),
              ),
              if (lastPoint != null)
                Text(
                  '${lastPoint.y.toStringAsFixed(6)}, ${lastPoint.x.toStringAsFixed(6)}',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: const Color(0xFF94A3B8),
                    fontSize: 11,
                  ),
                ),
            ],
          ),
          const SizedBox(height: 12),
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
          Text(
            'Du kannst Punkte per GPS speichern oder direkt in die Karte tippen.',
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: const Color(0xFFCBD5E1)),
          ),
          const SizedBox(height: 12),
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
                    foregroundColor: Colors.white,
                  ),
                  child: Column(
                    children: <Widget>[
                      const Text('Schließen'),
                      if (activePoints.length < minimumPoints)
                        Text(
                          'min. $minimumPoints Punkte',
                          style: Theme.of(context).textTheme.bodySmall
                              ?.copyWith(
                                fontSize: 10,
                                color: const Color(0xFF94A3B8),
                              ),
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
                    side: const BorderSide(color: Color(0xFFC6463B)),
                    foregroundColor: const Color(0xFFFECACA),
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

class _RoundMapButton extends StatelessWidget {
  const _RoundMapButton({required this.icon, required this.onTap});

  final IconData icon;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final foreground = onTap == null ? const Color(0xFF8A8A90) : Colors.white;

    return Material(
      color: const Color(0xFF3A3A3C),
      shape: const CircleBorder(),
      child: InkWell(
        customBorder: const CircleBorder(),
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Icon(icon, size: 20, color: foreground),
        ),
      ),
    );
  }
}

class _ActionTile extends StatelessWidget {
  const _ActionTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.accentColor,
    required this.foregroundColor,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final Color accentColor;
  final Color foregroundColor;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: accentColor,
      borderRadius: BorderRadius.circular(16),
      child: InkWell(
        borderRadius: BorderRadius.circular(16),
        onTap: onTap,
        child: SizedBox(
          width: 96,
          height: 112,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 14),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Icon(icon, size: 28, color: foregroundColor),
                const Spacer(),
                Text(
                  title,
                  style: Theme.of(context).textTheme.labelLarge?.copyWith(
                    color: foregroundColor,
                    fontWeight: FontWeight.w800,
                  ),
                ),
                Text(
                  subtitle,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: foregroundColor.withValues(alpha: 0.82),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _TitlePill extends StatelessWidget {
  const _TitlePill({required this.title, required this.subtitle});

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF2FFFFFF),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              title,
              style: Theme.of(
                context,
              ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w800),
            ),
            Text(
              subtitle,
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: const Color(0xFF667267)),
            ),
          ],
        ),
      ),
    );
  }
}

class _StatusPill extends StatelessWidget {
  const _StatusPill({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xE6FFFFFF),
        borderRadius: BorderRadius.circular(18),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Icon(icon, size: 16, color: const Color(0xFF1F2A22)),
            const SizedBox(width: 6),
            Text(label),
          ],
        ),
      ),
    );
  }
}

class _ModeChip extends StatelessWidget {
  const _ModeChip({
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return ChoiceChip(
      label: Text(label),
      selected: selected,
      onSelected: (_) => onTap(),
      selectedColor: const Color(0xFF2F7D4A),
      labelStyle: TextStyle(
        color: selected ? Colors.white : const Color(0xFF1F2A22),
        fontWeight: FontWeight.w700,
      ),
    );
  }
}

class _ObjectChip extends StatelessWidget {
  const _ObjectChip({
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return FilterChip(
      label: Text(label),
      selected: selected,
      onSelected: (_) => onTap(),
      selectedColor: const Color(0xFFDCE4D8),
      checkmarkColor: const Color(0xFF2F7D4A),
      labelStyle: const TextStyle(fontWeight: FontWeight.w600),
    );
  }
}

class _GpsRow extends StatelessWidget {
  const _GpsRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        children: <Widget>[
          Expanded(
            child: Text(
              label,
              style: Theme.of(
                context,
              ).textTheme.bodyMedium?.copyWith(color: const Color(0xFF667267)),
            ),
          ),
          const SizedBox(width: 12),
          Text(
            value,
            style: Theme.of(
              context,
            ).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700),
          ),
        ],
      ),
    );
  }
}

class _MapSizePill extends StatelessWidget {
  const _MapSizePill({
    required this.currentSquareMeters,
    required this.maxSquareMeters,
  });

  final int currentSquareMeters;
  final int maxSquareMeters;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF2FFFFFF),
        borderRadius: BorderRadius.circular(16),
        boxShadow: const <BoxShadow>[
          BoxShadow(
            color: Color(0x14000000),
            blurRadius: 12,
            offset: Offset(0, 4),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Text(
              'Kartengröße',
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: const Color(0xFF667267)),
            ),
            const SizedBox(height: 2),
            Text(
              '$currentSquareMeters / $maxSquareMeters m²',
              style: Theme.of(
                context,
              ).textTheme.titleSmall?.copyWith(fontWeight: FontWeight.w800),
            ),
          ],
        ),
      ),
    );
  }
}
