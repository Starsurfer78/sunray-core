import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import '../../domain/map/map_geometry.dart';
import '../../domain/robot/robot_status.dart';
import '../../features/map/domain/map_editor_state.dart';

class RobotMapView extends StatefulWidget {
  const RobotMapView({
    required this.map,
    this.status,
    this.selectedZoneIds = const <String>[],
    this.interactive = true,
    this.onMapTap,
    this.activePoints = const <MapPoint>[],
    this.segmentPoints = const <EditableSegmentTarget>[],
    this.onPointTap,
    this.onPointDragStart,
    this.onPointDragUpdate,
    this.onPointDragEnd,
    this.onSegmentTap,
    this.selectedPointIndex,
    this.showPerimeter = true,
    this.showNoGo = true,
    this.showZones = true,
    this.showDock = true,
    this.highlightActiveZoneId,
    this.previewRoutePoints = const <MapPoint>[],
    this.showCenterButton = false,
    this.onZoneTap,
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
  final void Function(int index)? onPointDragStart;
  final void Function(int index, MapPoint point)? onPointDragUpdate;
  final void Function()? onPointDragEnd;
  final void Function(int insertIndex)? onSegmentTap;
  final int? selectedPointIndex;
  final bool showPerimeter;
  final bool showNoGo;
  final bool showZones;
  final bool showDock;
  final String? highlightActiveZoneId;
  final List<MapPoint> previewRoutePoints;
  final bool showCenterButton;
  final void Function(String zoneId)? onZoneTap;

  @override
  State<RobotMapView> createState() => _RobotMapViewState();
}

class _RobotMapViewState extends State<RobotMapView> {
  late final MapController _mapController;
  bool _hasAutoCentered = false;
  bool _mapReady = false;

  static bool _isValidGps(double? lat, double? lon) =>
      lat != null && lon != null && (lat != 0.0 || lon != 0.0);

  @override
  void initState() {
    super.initState();
    _mapController = MapController();
  }

  @override
  void dispose() {
    _mapController.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(RobotMapView oldWidget) {
    super.didUpdateWidget(oldWidget);
    _tryAutoCenter();
  }

  void _onMapReady() {
    _mapReady = true;
    _tryAutoCenter();
  }

  void _tryAutoCenter() {
    if (_hasAutoCentered || !_mapReady) {
      return;
    }
    final lat = widget.status?.gpsLat;
    final lon = widget.status?.gpsLon;
    if (!_isValidGps(lat, lon)) {
      return;
    }
    _hasAutoCentered = true;
    _mapController.move(LatLng(lat!, lon!), 19);
  }

  void _centerOnRobot() {
    final lat = widget.status?.gpsLat;
    final lon = widget.status?.gpsLon;
    if (!_isValidGps(lat, lon)) {
      return;
    }
    _mapController.move(LatLng(lat!, lon!), 19);
  }

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

    final hasRobot = _isValidGps(widget.status?.gpsLat, widget.status?.gpsLon);

    return Stack(
      children: <Widget>[
        FlutterMap(
          mapController: _mapController,
          options: MapOptions(
            initialCenter: camera.initialCenter,
            initialZoom: camera.initialZoom,
            onMapReady: _onMapReady,
            interactionOptions: InteractionOptions(
              flags: widget.interactive
                  ? InteractiveFlag.all
                  : InteractiveFlag.none,
            ),
            onTap: (widget.onMapTap == null && widget.onZoneTap == null)
                ? null
                : (tapPosition, latLng) {
                    if (widget.onZoneTap != null) {
                      final zoneId = _findZoneAtPoint(latLng);
                      if (zoneId != null) {
                        widget.onZoneTap!.call(zoneId);
                        return;
                      }
                    }
                    widget.onMapTap?.call(
                      MapPoint(x: latLng.longitude, y: latLng.latitude),
                    );
                  },
          ),
          children: <Widget>[
            TileLayer(
              urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
              userAgentPackageName: 'sunray.mobile',
              maxZoom: 21,
            ),
            if (widget.showPerimeter && widget.map.perimeter.length >= 2)
              PolylineLayer(
                polylines: <Polyline<Object>>[
                  Polyline<Object>(
                    points: _closed(widget.map.perimeter),
                    color: const Color(0xFF2F7D4A),
                    strokeWidth: 3,
                  ),
                ],
              ),
            if (widget.showPerimeter && widget.map.perimeter.length >= 3)
              PolygonLayer(
                polygons: <Polygon<Object>>[
                  Polygon<Object>(
                    points: _points(widget.map.perimeter),
                    color: const Color(0x1F2F7D4A),
                    borderColor: const Color(0xFF2F7D4A),
                    borderStrokeWidth: 2,
                  ),
                ],
              ),
            if (widget.showZones)
              PolygonLayer(
                polygons: widget.map.zones
                    .where((zone) => zone.points.length >= 3)
                    .map(
                      (zone) => Polygon<Object>(
                        points: _points(zone.points),
                        color: _zoneFill(zone.id),
                        borderColor: _zoneBorder(zone.id),
                        borderStrokeWidth:
                            zone.id == widget.highlightActiveZoneId ? 3 : 2,
                      ),
                    )
                    .toList(growable: false),
              ),
            if (widget.showNoGo)
              PolygonLayer(
                polygons: widget.map.noGoZones
                    .where((polygon) => polygon.length >= 3)
                    .map(
                      (polygon) => Polygon<Object>(
                        points: _points(polygon),
                        color: const Color(0x22C6463B),
                        borderColor: const Color(0xFFC6463B),
                        borderStrokeWidth: 2,
                      ),
                    )
                    .toList(growable: false),
              ),
            if (widget.showDock && widget.map.dock.length >= 2)
              PolylineLayer(
                polylines: <Polyline<Object>>[
                  Polyline<Object>(
                    points: _points(widget.map.dock),
                    color: const Color(0xFFF59E0B),
                    strokeWidth: 3,
                  ),
                ],
              ),
            if (widget.previewRoutePoints.length >= 2)
              PolylineLayer(
                polylines: <Polyline<Object>>[
                  Polyline<Object>(
                    points: _points(widget.previewRoutePoints),
                    color: const Color(0xFFD6D94E),
                    strokeWidth: 3.5,
                  ),
                ],
              ),
            if (widget.segmentPoints.isNotEmpty)
              MarkerLayer(
                markers: widget.segmentPoints
                    .map(
                      (segment) => Marker(
                        point: _latLng(segment.point),
                        width: 20,
                        height: 20,
                        child: GestureDetector(
                          onTap: () =>
                              widget.onSegmentTap?.call(segment.insertIndex),
                          child: Container(
                            decoration: BoxDecoration(
                              color: const Color(0xFF1C1C1E),
                              borderRadius: BorderRadius.circular(10),
                              border: Border.all(
                                color: const Color(0xFFDCE4D8),
                              ),
                            ),
                            child: const Icon(
                              Icons.add,
                              size: 14,
                              color: Color(0xFFDCE4D8),
                            ),
                          ),
                        ),
                      ),
                    )
                    .toList(growable: false),
              ),
            if (widget.activePoints.isNotEmpty)
              MarkerLayer(
                markers: widget.activePoints
                    .asMap()
                    .entries
                    .map(
                      (entry) => Marker(
                        point: _latLng(entry.value),
                        width: 24,
                        height: 24,
                        child: Builder(
                          builder: (context) {
                            final camera = MapCamera.of(context);
                            return GestureDetector(
                              onTap: () => widget.onPointTap?.call(entry.key),
                              onPanStart: (_) =>
                                  widget.onPointDragStart?.call(entry.key),
                              onPanUpdate: (details) {
                                final currentScreenPoint = camera
                                    .latLngToScreenPoint(_latLng(entry.value));
                                final nextScreenPoint = ui.Offset(
                                  currentScreenPoint.x + details.delta.dx,
                                  currentScreenPoint.y + details.delta.dy,
                                );
                                final nextLatLng = camera.pointToLatLng(
                                  math.Point<double>(
                                    nextScreenPoint.dx,
                                    nextScreenPoint.dy,
                                  ),
                                );
                                widget.onPointDragUpdate?.call(
                                  entry.key,
                                  MapPoint(
                                    x: nextLatLng.longitude,
                                    y: nextLatLng.latitude,
                                  ),
                                );
                              },
                              onPanEnd: (_) => widget.onPointDragEnd?.call(),
                              onPanCancel: () => widget.onPointDragEnd?.call(),
                              child: Container(
                                decoration: BoxDecoration(
                                  color: entry.key == widget.selectedPointIndex
                                      ? const Color(0xFFF59E0B)
                                      : const Color(0xFF2563EB),
                                  shape: BoxShape.circle,
                                  border: Border.all(
                                    color: Colors.white,
                                    width: 2,
                                  ),
                                ),
                              ),
                            );
                          },
                        ),
                      ),
                    )
                    .toList(growable: false),
              ),
            if (hasRobot)
              MarkerLayer(
                markers: <Marker>[
                  Marker(
                    point: LatLng(
                      widget.status!.gpsLat!,
                      widget.status!.gpsLon!,
                    ),
                    width: 34,
                    height: 34,
                    child: Transform.rotate(
                      angle: widget.status!.heading ?? 0,
                      child: const Icon(
                        Icons.navigation_rounded,
                        color: Color(0xFFD6D94E),
                        size: 28,
                      ),
                    ),
                  ),
                ],
              ),
          ],
        ),
        if (widget.showCenterButton && hasRobot)
          Positioned(
            bottom: 16,
            right: 16,
            child: FloatingActionButton.small(
              heroTag: 'center_robot',
              onPressed: _centerOnRobot,
              tooltip: 'Roboter zentrieren',
              backgroundColor: const Color(0xFF1F2A22),
              child: const Icon(Icons.my_location_rounded),
            ),
          ),
      ],
    );
  }

  String? _findZoneAtPoint(LatLng latLng) {
    for (final zone in widget.map.zones) {
      if (zone.points.length >= 3 && _pointInPolygon(latLng, zone.points)) {
        return zone.id;
      }
    }
    return null;
  }

  bool _pointInPolygon(LatLng point, List<MapPoint> polygon) {
    bool inside = false;
    final px = point.longitude;
    final py = point.latitude;
    final count = polygon.length;
    for (int index = 0, prev = count - 1; index < count; prev = index++) {
      final currentX = polygon[index].x;
      final currentY = polygon[index].y;
      final prevX = polygon[prev].x;
      final prevY = polygon[prev].y;
      if (((currentY > py) != (prevY > py)) &&
          (px <
              (prevX - currentX) * (py - currentY) / (prevY - currentY) +
                  currentX)) {
        inside = !inside;
      }
    }
    return inside;
  }

  Color _zoneFill(String zoneId) {
    if (zoneId == widget.highlightActiveZoneId) {
      return const Color(0x3354A065);
    }
    if (widget.selectedZoneIds.contains(zoneId)) {
      return const Color(0x2254A065);
    }
    return const Color(0x1854A065);
  }

  Color _zoneBorder(String zoneId) {
    if (zoneId == widget.highlightActiveZoneId) {
      return const Color(0xFF2F7D4A);
    }
    if (widget.selectedZoneIds.contains(zoneId)) {
      return const Color(0xFF3E8B57);
    }
    return const Color(0xFF54A065);
  }

  LatLngBounds? _computeBounds() {
    final allPoints = <MapPoint>[
      if (widget.showPerimeter) ...widget.map.perimeter,
      if (widget.showDock) ...widget.map.dock,
      if (widget.showNoGo)
        for (final polygon in widget.map.noGoZones) ...polygon,
      if (widget.showZones)
        for (final zone in widget.map.zones) ...zone.points,
      ...widget.activePoints,
      ...widget.segmentPoints.map((segment) => segment.point),
      if (_isValidGps(widget.status?.gpsLat, widget.status?.gpsLon))
        MapPoint(x: widget.status!.gpsLon!, y: widget.status!.gpsLat!),
      ...widget.previewRoutePoints,
    ];
    if (allPoints.isEmpty) {
      return null;
    }
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
    if (points.isEmpty) {
      return const <LatLng>[];
    }
    final projected = _points(points);
    return <LatLng>[...projected, projected.first];
  }

  LatLng _latLng(MapPoint point) => LatLng(point.y, point.x);

  double _zoomForBounds(LatLngBounds bounds) {
    final latSpan = (bounds.north - bounds.south).abs();
    final lngSpan = (bounds.east - bounds.west).abs();
    final span = math.max(latSpan, lngSpan);
    if (span <= 0.0002) {
      return 21;
    }
    if (span <= 0.0005) {
      return 20;
    }
    if (span <= 0.001) {
      return 19;
    }
    if (span <= 0.003) {
      return 18;
    }
    if (span <= 0.01) {
      return 17;
    }
    if (span <= 0.03) {
      return 16;
    }
    if (span <= 0.1) {
      return 15;
    }
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
