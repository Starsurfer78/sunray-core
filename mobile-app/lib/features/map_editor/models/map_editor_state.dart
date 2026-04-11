import '../../../domain/map/map_geometry.dart';

enum MapEditorMode {
  view,
  record,
  edit,
}

enum EditableMapObjectType {
  perimeter,
  noGo,
  zone,
  dock,
}

enum MapSetupStage {
  perimeter,
  noGo,
  dock,
  validate,
  save,
}

class MapEditorState {
  const MapEditorState({
    this.mode = MapEditorMode.view,
    this.activeObject = EditableMapObjectType.perimeter,
    this.setupStage = MapSetupStage.perimeter,
    this.activeZoneId,
    this.activeNoGoIndex,
    this.selectedPointIndex,
    this.showPerimeter = true,
    this.showNoGo = true,
    this.showZones = true,
    this.showDock = true,
  });

  final MapEditorMode mode;
  final EditableMapObjectType activeObject;
  final MapSetupStage setupStage;
  final String? activeZoneId;
  final int? activeNoGoIndex;
  final int? selectedPointIndex;
  final bool showPerimeter;
  final bool showNoGo;
  final bool showZones;
  final bool showDock;

  bool get canEditPoints => mode == MapEditorMode.record || mode == MapEditorMode.edit;

  MapEditorState copyWith({
    MapEditorMode? mode,
    EditableMapObjectType? activeObject,
    MapSetupStage? setupStage,
    String? activeZoneId,
    int? activeNoGoIndex,
    int? selectedPointIndex,
    bool? showPerimeter,
    bool? showNoGo,
    bool? showZones,
    bool? showDock,
    bool clearActiveZone = false,
    bool clearActiveNoGo = false,
    bool clearSelectedPoint = false,
  }) {
    return MapEditorState(
      mode: mode ?? this.mode,
      activeObject: activeObject ?? this.activeObject,
      setupStage: setupStage ?? this.setupStage,
      activeZoneId: clearActiveZone ? null : (activeZoneId ?? this.activeZoneId),
      activeNoGoIndex: clearActiveNoGo ? null : (activeNoGoIndex ?? this.activeNoGoIndex),
      selectedPointIndex: clearSelectedPoint ? null : (selectedPointIndex ?? this.selectedPointIndex),
      showPerimeter: showPerimeter ?? this.showPerimeter,
      showNoGo: showNoGo ?? this.showNoGo,
      showZones: showZones ?? this.showZones,
      showDock: showDock ?? this.showDock,
    );
  }
}

class EditablePointTarget {
  const EditablePointTarget({
    required this.index,
    required this.point,
  });

  final int index;
  final MapPoint point;
}

class EditableSegmentTarget {
  const EditableSegmentTarget({
    required this.insertIndex,
    required this.point,
  });

  final int insertIndex;
  final MapPoint point;
}
