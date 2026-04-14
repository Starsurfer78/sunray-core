class MapGeometry {
  const MapGeometry({
    this.hasPerimeter = false,
    this.hasDock = false,
    this.zoneCount = 0,
    this.noGoCount = 0,
    this.perimeter = const <MapPoint>[],
    this.noGoZones = const <List<MapPoint>>[],
    this.dock = const <MapPoint>[],
    this.zones = const <ZoneGeometry>[],
  });

  final bool hasPerimeter;
  final bool hasDock;
  final int zoneCount;
  final int noGoCount;
  final List<MapPoint> perimeter;
  final List<List<MapPoint>> noGoZones;
  final List<MapPoint> dock;
  final List<ZoneGeometry> zones;

  MapGeometry copyWith({
    bool? hasPerimeter,
    bool? hasDock,
    int? zoneCount,
    int? noGoCount,
    List<MapPoint>? perimeter,
    List<List<MapPoint>>? noGoZones,
    List<MapPoint>? dock,
    List<ZoneGeometry>? zones,
  }) {
    final nextPerimeter = perimeter ?? this.perimeter;
    final nextDock = dock ?? this.dock;
    final nextNoGoZones = noGoZones ?? this.noGoZones;
    final nextZones = zones ?? this.zones;

    return MapGeometry(
      hasPerimeter: hasPerimeter ?? nextPerimeter.length >= 3,
      hasDock: hasDock ?? nextDock.isNotEmpty,
      zoneCount: zoneCount ?? nextZones.length,
      noGoCount: noGoCount ?? nextNoGoZones.length,
      perimeter: nextPerimeter,
      noGoZones: nextNoGoZones,
      dock: nextDock,
      zones: nextZones,
    );
  }
}

MapPoint mapPointFromDynamic(dynamic raw) {
  if (raw is Map<String, dynamic>) {
    return MapPoint(
      x: (raw['x'] as num?)?.toDouble() ?? 0,
      y: (raw['y'] as num?)?.toDouble() ?? 0,
    );
  }
  if (raw is List && raw.length >= 2) {
    return MapPoint(
      x: (raw[0] as num?)?.toDouble() ?? 0,
      y: (raw[1] as num?)?.toDouble() ?? 0,
    );
  }
  return const MapPoint(x: 0, y: 0);
}

List<MapPoint> mapPointsFromDynamic(dynamic raw) {
  if (raw is! List) return const <MapPoint>[];
  return raw.map(mapPointFromDynamic).toList(growable: false);
}

List<dynamic> encodeMapPoints(List<MapPoint> points) {
  return points
      .map((point) => <double>[point.x, point.y])
      .toList(growable: false);
}

class MapPoint {
  const MapPoint({
    required this.x,
    required this.y,
  });

  final double x;
  final double y;

  MapPoint copyWith({
    double? x,
    double? y,
  }) {
    return MapPoint(
      x: x ?? this.x,
      y: y ?? this.y,
    );
  }
}

class ZoneGeometry {
  const ZoneGeometry({
    required this.id,
    required this.name,
    required this.points,
    this.priority = 1,
    this.mowingDirection = 'Automatisch',
    this.mowingProfile = 'Standard',
  });

  final String id;
  final String name;
  final List<MapPoint> points;
  final int priority;
  final String mowingDirection;
  final String mowingProfile;

  ZoneGeometry copyWith({
    String? id,
    String? name,
    List<MapPoint>? points,
    int? priority,
    String? mowingDirection,
    String? mowingProfile,
  }) {
    return ZoneGeometry(
      id: id ?? this.id,
      name: name ?? this.name,
      points: points ?? this.points,
      priority: priority ?? this.priority,
      mowingDirection: mowingDirection ?? this.mowingDirection,
      mowingProfile: mowingProfile ?? this.mowingProfile,
    );
  }
}