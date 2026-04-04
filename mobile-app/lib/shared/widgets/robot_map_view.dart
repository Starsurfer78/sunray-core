import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import '../../domain/map/map_geometry.dart';
import '../../domain/robot/robot_status.dart';
import '../../features/map_editor/models/map_editor_state.dart';

class RobotMapView extends StatelessWidget {
  const RobotMapView({
    required this.map,
    this.status,
    this.selectedZoneIds = const <String>[],
    this.interactive = true,
    this.onMapTap,
    this.activePoints = const <MapPoint>[],
    this.segmentPoints = const <EditableSegmentTarget>[],
    this.onPointTap,
    this.onSegmentTap,
    this.selectedPointIndex,
    this.showPerimeter = true,
    this.showNoGo = true,
    this.showZones = true,
    this.showDock = true,
    this.highlightActiveZoneId,
    this.previewRoutePoints = const <MapPoint>[],
    super.key,
  });

  final MapGeometry map;
  final RobotStatus? status;
  final List<String> selectedZoneIds;
  final bool interactive;
  final void Function(MapPoint point)? onMapTap;
  final List<MapPoint> activePoints;
  final List<EditableSegmentTarget> segmentPoints;
  final void Function(int index)? onPointTap;
  final void Function(int insertIndex)? onSegmentTap;
  final int? selectedPointIndex;
  final bool showPerimeter;
  final bool showNoGo;
  final bool showZones;
  final bool showDock;
  final String? highlightActiveZoneId;
  final List<MapPoint> previewRoutePoints;

  @override
  Widget build(BuildContext context) {
    final bounds = _computeBounds();
    final camera = bounds == null
        ? const MapCameraOptions(
            initialCenter: LatLng(52.52, 13.4),
            initialZoom: 18,
          )
        : MapCameraOptions(
            initialCenter: bounds.center,
            initialZoom: _zoomForBounds(bounds),
          );

    return FlutterMap(
      options: MapOptions(
        initialCenter: camera.initialCenter,
        initialZoom: camera.initialZoom,
        interactionOptions: InteractionOptions(
          flags: interactive
              ? InteractiveFlag.all
              : InteractiveFlag.none,
        ),
        onTap: onMapTap == null
            ? null
            : (tapPosition, latLng) {
                onMapTap!.call(MapPoint(x: latLng.longitude, y: latLng.latitude));
              },
      ),
      children: <Widget>[
        TileLayer(
          urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
          userAgentPackageName: 'sunray.mobile',
          maxZoom: 21,
        ),
        if (showPerimeter && map.perimeter.length >= 2)
          PolylineLayer(
            polylines: <Polyline<Object>>[
              Polyline<Object>(
                points: _closed(map.perimeter),
                color: const Color(0xFF22C55E),
                strokeWidth: 3,
              ),
            ],
          ),
        if (showPerimeter && map.perimeter.length >= 3)
          PolygonLayer(
            polygons: <Polygon<Object>>[
              Polygon<Object>(
                points: _points(map.perimeter),
                color: const Color(0x1F22C55E),
                borderColor: const Color(0xFF22C55E),
                borderStrokeWidth: 2,
              ),
            ],
          ),
        if (showZones)
          PolygonLayer(
            polygons: map.zones
                .where((zone) => zone.points.length >= 3)
                .map(
                  (zone) => Polygon<Object>(
                    points: _points(zone.points),
                    color: _zoneFill(zone.id),
                    borderColor: _zoneBorder(zone.id),
                    borderStrokeWidth: zone.id == highlightActiveZoneId ? 3 : 2,
                  ),
                )
                .toList(growable: false),
          ),
        if (showNoGo)
          PolygonLayer(
            polygons: map.noGoZones
                .where((polygon) => polygon.length >= 3)
                .map(
                  (polygon) => Polygon<Object>(
                    points: _points(polygon),
                    color: const Color(0x22EF4444),
                    borderColor: const Color(0xFFF87171),
                    borderStrokeWidth: 2,
                  ),
                )
                .toList(growable: false),
          ),
        if (showDock && map.dock.length >= 2)
          PolylineLayer(
            polylines: <Polyline<Object>>[
              Polyline<Object>(
                points: _points(map.dock),
                color: const Color(0xFFF59E0B),
                strokeWidth: 3,
              ),
            ],
          ),
        if (previewRoutePoints.length >= 2)
          PolylineLayer(
            polylines: <Polyline<Object>>[
              Polyline<Object>(
                points: _points(previewRoutePoints),
                color: const Color(0xFFFACC15),
                strokeWidth: 3.5,
              ),
            ],
          ),
        if (segmentPoints.isNotEmpty)
          MarkerLayer(
            markers: segmentPoints
                .map(
                  (segment) => Marker(
                    point: _latLng(segment.point),
                    width: 20,
                    height: 20,
                    child: GestureDetector(
                      onTap: () => onSegmentTap?.call(segment.insertIndex),
                      child: Container(
                        decoration: BoxDecoration(
                          color: const Color(0xFF0F172A),
                          borderRadius: BorderRadius.circular(10),
                          border: Border.all(color: const Color(0xFF93C5FD)),
                        ),
                        child: const Icon(Icons.add, size: 14, color: Color(0xFF93C5FD)),
                      ),
                    ),
                  ),
                )
                .toList(growable: false),
          ),
        if (activePoints.isNotEmpty)
          MarkerLayer(
            markers: activePoints
                .asMap()
                .entries
                .map(
                  (entry) => Marker(
                    point: _latLng(entry.value),
                    width: 24,
                    height: 24,
                    child: GestureDetector(
                      onTap: () => onPointTap?.call(entry.key),
                      child: Container(
                        decoration: BoxDecoration(
                          color: entry.key == selectedPointIndex
                              ? const Color(0xFFF59E0B)
                              : const Color(0xFF2563EB),
                          shape: BoxShape.circle,
                          border: Border.all(color: Colors.white, width: 2),
                        ),
                      ),
                    ),
                  ),
                )
                .toList(growable: false),
          ),
        if (status?.gpsLat != null && status?.gpsLon != null)
          MarkerLayer(
            markers: <Marker>[
              Marker(
                point: LatLng(status!.gpsLat!, status!.gpsLon!),
                width: 34,
                height: 34,
                child: Transform.rotate(
                  angle: status!.heading ?? 0,
                  child: const Icon(
                    Icons.navigation_rounded,
                    color: Color(0xFFFACC15),
                    size: 28,
                  ),
                ),
              ),
            ],
          ),
      ],
    );
  }

  Color _zoneFill(String zoneId) {
    if (zoneId == highlightActiveZoneId) return const Color(0x3B8B5CF6);
    if (selectedZoneIds.contains(zoneId)) return const Color(0x268B5CF6);
    return const Color(0x182563EB);
  }

  Color _zoneBorder(String zoneId) {
    if (zoneId == highlightActiveZoneId) return const Color(0xFFA78BFA);
    if (selectedZoneIds.contains(zoneId)) return const Color(0xFF60A5FA);
    return const Color(0xFF3B82F6);
  }

  LatLngBounds? _computeBounds() {
    final allPoints = <MapPoint>[
      if (showPerimeter) ...map.perimeter,
      if (showDock) ...map.dock,
      if (showNoGo) for (final polygon in map.noGoZones) ...polygon,
      if (showZones) for (final zone in map.zones) ...zone.points,
      ...activePoints,
      ...segmentPoints.map((segment) => segment.point),
      if (status?.gpsLat != null && status?.gpsLon != null)
        MapPoint(x: status!.gpsLon!, y: status!.gpsLat!),
      ...previewRoutePoints,
    ];
    if (allPoints.isEmpty) return null;
    final lats = allPoints.map((point) => point.y);
    final lngs = allPoints.map((point) => point.x);
    return LatLngBounds(
      LatLng(lats.reduce(math.min), lngs.reduce(math.min)),
      LatLng(lats.reduce(math.max), lngs.reduce(math.max)),
    );
  }

  List<LatLng> _points(List<MapPoint> points) {
    return points.map(_latLng).toList(growable: false);
  }

  List<LatLng> _closed(List<MapPoint> points) {
    if (points.isEmpty) return const <LatLng>[];
    final projected = _points(points);
    return <LatLng>[...projected, projected.first];
  }

  LatLng _latLng(MapPoint point) => LatLng(point.y, point.x);

  double _zoomForBounds(LatLngBounds bounds) {
    final latSpan = (bounds.north - bounds.south).abs();
    final lngSpan = (bounds.east - bounds.west).abs();
    final span = math.max(latSpan, lngSpan);
    if (span <= 0.0002) return 21;
    if (span <= 0.0005) return 20;
    if (span <= 0.001) return 19;
    if (span <= 0.003) return 18;
    if (span <= 0.01) return 17;
    if (span <= 0.03) return 16;
    if (span <= 0.1) return 15;
    return 14;
  }
}

class MapCameraOptions {
  const MapCameraOptions({
    required this.initialCenter,
    required this.initialZoom,
  });

  final LatLng initialCenter;
  final double initialZoom;
}
