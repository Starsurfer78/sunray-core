import '../../domain/map/map_geometry.dart';
import '../../domain/map/stored_map.dart';
import '../../domain/robot/robot_status.dart';
import 'robot_api.dart';
import 'robot_ws.dart';

class RobotRepository {
  RobotRepository({required RobotApi api, required RobotWsClient ws})
    : _api = api,
      _ws = ws;

  final RobotApi _api;
  final RobotWsClient _ws;

  RobotApi get api => _api;
  RobotWsClient get ws => _ws;

  Future<RobotStatus> probeRobot({
    required String host,
    required int port,
    String? fallbackName,
  }) async {
    try {
      final map = await _api.getMap(host: host, port: port);
      return RobotStatus(
        connectionState: ConnectionStateKind.connected,
        robotName: _extractRobotName(map) ?? fallbackName ?? host,
      );
    } catch (error) {
      return RobotStatus(
        connectionState: ConnectionStateKind.error,
        robotName: fallbackName ?? host,
        lastError: 'Verbindung fehlgeschlagen: $error',
      );
    }
  }

  Future<MapGeometry> fetchMapGeometry({
    required String host,
    required int port,
  }) async {
    final map = await _api.getMap(host: host, port: port);
    final perimeter = mapPointsFromDynamic(map['perimeter']);
    final dock = mapPointsFromDynamic(map['dock']);
    final exclusions = _parsePolygonList(map['exclusions']);
    final zones = _parseZones(map['zones']);

    return MapGeometry(
      hasPerimeter: perimeter.isNotEmpty,
      hasDock: dock.isNotEmpty,
      zoneCount: zones.length,
      noGoCount: exclusions.length,
      perimeter: perimeter,
      dock: dock,
      noGoZones: exclusions,
      zones: zones,
    );
  }

  Future<void> saveMapGeometry({
    required String host,
    required int port,
    required MapGeometry geometry,
  }) {
    return _api.saveMap(
      host: host,
      port: port,
      payload: encodeMapGeometry(geometry),
    );
  }

  Future<({String activeId, List<StoredMap> maps})> fetchStoredMaps({
    required String host,
    required int port,
  }) async {
    final raw = await _api.getStoredMaps(host: host, port: port);
    final activeId = raw['active_id'] as String? ?? '';
    final maps =
        (raw['maps'] as List<dynamic>? ?? const <dynamic>[])
            .whereType<Map<String, dynamic>>()
            .map(
              (entry) => StoredMap(
                id: entry['id'] as String? ?? '',
                name: entry['name'] as String? ?? 'Karte',
                createdAtMs: entry['created_at_ms'] as int?,
                updatedAtMs: entry['updated_at_ms'] as int?,
              ),
            )
            .where((m) => m.id.isNotEmpty)
            .toList(growable: false);
    return (activeId: activeId, maps: maps);
  }

  Future<void> createNamedMap({
    required String host,
    required int port,
    required String name,
    required MapGeometry geometry,
  }) async {
    await _api.createStoredMap(
      host: host,
      port: port,
      payload: <String, dynamic>{
        'name': name,
        'activate': true,
        'map': encodeMapGeometry(geometry),
      },
    );
  }

  Future<void> activateStoredMapById({
    required String host,
    required int port,
    required String mapId,
  }) {
    return _api.activateStoredMap(host: host, port: port, mapId: mapId);
  }

  Future<List<MapPoint>> fetchPlannerPreview({
    required String host,
    required int port,
    required MapGeometry geometry,
    required List<String> zoneIds,
  }) async {
    final response = await _api.previewPlanner(
      host: host,
      port: port,
      payload: <String, dynamic>{
        'map': encodeMapGeometry(geometry),
        'mission': <String, dynamic>{
          'id': zoneIds.isEmpty ? 'preview-full-map' : 'preview-zones',
          'name': zoneIds.isEmpty ? 'Vollkarte' : 'Zonenvorschau',
          'zoneIds': zoneIds,
        },
      },
    );
    final routes = response['routes'];
    if (routes is! List || routes.isEmpty) {
      throw const FormatException('Planner-Vorschau enthält keine Route.');
    }
    final route = routes.first;
    if (route is! Map<String, dynamic>) {
      throw const FormatException(
        'Planner-Vorschau hat ein ungültiges Format.',
      );
    }
    if (route['valid'] == false) {
      throw FormatException(
        route['error'] as String? ?? 'Planner-Vorschau ist ungültig.',
      );
    }
    final waypoints = route['waypoints'];
    if (waypoints is! List) {
      throw const FormatException('Planner-Vorschau enthält keine Wegpunkte.');
    }
    final points = waypoints
        .whereType<Map<String, dynamic>>()
        .map(
          (waypoint) => MapPoint(
            x: (waypoint['x'] as num?)?.toDouble() ?? 0,
            y: (waypoint['y'] as num?)?.toDouble() ?? 0,
          ),
        )
        .toList(growable: false);
    if (points.length < 2) {
      throw const FormatException('Planner-Vorschau ist leer.');
    }
    return points;
  }

  String? _extractRobotName(Map<String, dynamic> map) {
    final robot = map['robot'];
    if (robot is Map<String, dynamic>) {
      final name = robot['name'];
      if (name is String && name.trim().isNotEmpty) return name.trim();
    }
    return null;
  }

  List<List<MapPoint>> _parsePolygonList(dynamic raw) {
    if (raw is! List) return const <List<MapPoint>>[];
    return raw
        .whereType<List>()
        .map(mapPointsFromDynamic)
        .where((polygon) => polygon.isNotEmpty)
        .toList(growable: false);
  }

  List<ZoneGeometry> _parseZones(dynamic raw) {
    if (raw is! List) return const <ZoneGeometry>[];
    return raw
        .whereType<Map<String, dynamic>>()
        .map(
          (zone) => ZoneGeometry(
            id: zone['id'] as String? ?? '',
            name:
                (zone['settings'] as Map<String, dynamic>?)?['name']
                    as String? ??
                zone['name'] as String? ??
                'Zone',
            points: mapPointsFromDynamic(zone['polygon']),
            priority:
                (zone['settings'] as Map<String, dynamic>?)?['priority']
                    as int? ??
                1,
            mowingDirection:
                (zone['settings'] as Map<String, dynamic>?)?['mowingDirection']
                    as String? ??
                'Automatisch',
            mowingProfile:
                (zone['settings'] as Map<String, dynamic>?)?['mowingProfile']
                    as String? ??
                'Standard',
          ),
        )
        .where((zone) => zone.points.isNotEmpty)
        .toList(growable: false);
  }

  Map<String, dynamic> encodeMapGeometry(MapGeometry geometry) {
    final zones = geometry.zones
        .asMap()
        .entries
        .map((entry) {
          final index = entry.key;
          final zone = entry.value;
          return <String, dynamic>{
            'id': zone.id,
            'name': zone.name,
            'order': index + 1,
            'polygon': encodeMapPoints(zone.points),
            'settings': <String, dynamic>{
              'name': zone.name,
              'priority': zone.priority,
              'mowingDirection': zone.mowingDirection,
              'mowingProfile': zone.mowingProfile,
              'stripWidth': 0.18,
              'angle': switch (zone.mowingDirection) {
                'Nord-Sued' => 90.0,
                'Ost-West' => 0.0,
                'Diagonal' => 45.0,
                _ => 0.0,
              },
              'edgeMowing': true,
              'edgeRounds': 1,
              'speed': switch (zone.mowingProfile) {
                'Schnell' => 1.2,
                'Sorgfaeltig' => 0.8,
                _ => 1.0,
              },
              'pattern': 'stripe',
              'reverseAllowed': false,
              'clearance': 0.25,
            },
          };
        })
        .toList(growable: false);

    return <String, dynamic>{
      'perimeter': encodeMapPoints(geometry.perimeter),
      'dock': encodeMapPoints(geometry.dock),
      'exclusions': geometry.noGoZones
          .map(encodeMapPoints)
          .toList(growable: false),
      'zones': zones,
      'mow': const <dynamic>[],
    };
  }
}
