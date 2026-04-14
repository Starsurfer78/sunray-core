import '../../../domain/map/map_geometry.dart';
import 'map_editor_state.dart';

class MapEditorController {
  const MapEditorController();

  MapGeometry addZone(MapGeometry geometry, {String? name}) {
    final maxIdx = geometry.zones
        .map((zone) => int.tryParse(zone.id.replaceFirst('zone-', '')) ?? 0)
        .fold(0, (current, next) => current > next ? current : next);
    final nextIndex = maxIdx + 1;
    final zone = ZoneGeometry(
      id: 'zone-$nextIndex',
      name: name ?? 'Zone $nextIndex',
      points: const <MapPoint>[],
    );
    return geometry.copyWith(zones: <ZoneGeometry>[...geometry.zones, zone]);
  }

  MapGeometry addNoGo(MapGeometry geometry) {
    return geometry.copyWith(
      noGoZones: <List<MapPoint>>[
        ...geometry.noGoZones,
        const <MapPoint>[],
      ],
    );
  }

  MapGeometry addPoint(
    MapGeometry geometry,
    MapEditorState state,
    MapPoint point,
  ) {
    return _updateActivePoints(
      geometry,
      state,
      (points) => <MapPoint>[...points, point],
    );
  }

  MapGeometry insertPoint(
    MapGeometry geometry,
    MapEditorState state,
    int insertIndex,
    MapPoint point,
  ) {
    return _updateActivePoints(
      geometry,
      state,
      (points) {
        final next = <MapPoint>[...points];
        final safeIndex = insertIndex.clamp(0, next.length);
        next.insert(safeIndex, point);
        return next;
      },
    );
  }

  MapGeometry movePoint(
    MapGeometry geometry,
    MapEditorState state,
    int pointIndex,
    MapPoint point,
  ) {
    return _updateActivePoints(
      geometry,
      state,
      (points) {
        if (pointIndex < 0 || pointIndex >= points.length) {
          return points;
        }
        final next = <MapPoint>[...points];
        next[pointIndex] = point;
        return next;
      },
    );
  }

  MapGeometry deletePoint(
    MapGeometry geometry,
    MapEditorState state,
    int pointIndex,
  ) {
    return _updateActivePoints(
      geometry,
      state,
      (points) {
        if (pointIndex < 0 || pointIndex >= points.length) {
          return points;
        }
        final next = <MapPoint>[...points]..removeAt(pointIndex);
        return next;
      },
    );
  }

  MapGeometry deleteActiveObject(MapGeometry geometry, MapEditorState state) {
    switch (state.activeObject) {
      case EditableMapObjectType.perimeter:
        return geometry.copyWith(perimeter: const <MapPoint>[]);
      case EditableMapObjectType.dock:
        return geometry.copyWith(dock: const <MapPoint>[]);
      case EditableMapObjectType.noGo:
        final index = state.activeNoGoIndex;
        if (index == null || index < 0 || index >= geometry.noGoZones.length) {
          return geometry;
        }
        final next = <List<MapPoint>>[...geometry.noGoZones]..removeAt(index);
        return geometry.copyWith(noGoZones: next);
      case EditableMapObjectType.zone:
        final zoneId = state.activeZoneId;
        if (zoneId == null) {
          return geometry;
        }
        return geometry.copyWith(
          zones: geometry.zones.where((zone) => zone.id != zoneId).toList(growable: false),
        );
    }
  }

  List<MapPoint> activePoints(MapGeometry geometry, MapEditorState state) {
    switch (state.activeObject) {
      case EditableMapObjectType.perimeter:
        return geometry.perimeter;
      case EditableMapObjectType.dock:
        return geometry.dock;
      case EditableMapObjectType.noGo:
        final index = state.activeNoGoIndex;
        if (index == null || index < 0 || index >= geometry.noGoZones.length) {
          return const <MapPoint>[];
        }
        return geometry.noGoZones[index];
      case EditableMapObjectType.zone:
        final zoneId = state.activeZoneId;
        if (zoneId == null) {
          return const <MapPoint>[];
        }
        return geometry.zones.firstWhere(
          (zone) => zone.id == zoneId,
          orElse: () => const ZoneGeometry(id: '', name: '', points: <MapPoint>[]),
        ).points;
    }
  }

  MapGeometry _updateActivePoints(
    MapGeometry geometry,
    MapEditorState state,
    List<MapPoint> Function(List<MapPoint> points) mutate,
  ) {
    switch (state.activeObject) {
      case EditableMapObjectType.perimeter:
        return geometry.copyWith(perimeter: mutate(geometry.perimeter));
      case EditableMapObjectType.dock:
        return geometry.copyWith(dock: mutate(geometry.dock));
      case EditableMapObjectType.noGo:
        final index = state.activeNoGoIndex;
        if (index == null || index < 0 || index >= geometry.noGoZones.length) {
          return geometry;
        }
        final next = <List<MapPoint>>[...geometry.noGoZones];
        next[index] = mutate(next[index]);
        return geometry.copyWith(noGoZones: next);
      case EditableMapObjectType.zone:
        final zoneId = state.activeZoneId;
        if (zoneId == null) {
          return geometry;
        }
        final nextZones = geometry.zones
            .map((zone) => zone.id == zoneId ? zone.copyWith(points: mutate(zone.points)) : zone)
            .toList(growable: false);
        return geometry.copyWith(zones: nextZones);
    }
  }
}