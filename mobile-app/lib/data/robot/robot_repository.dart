import '../../domain/map/map_geometry.dart';
import '../../domain/robot/robot_status.dart';
import 'robot_api.dart';
import 'robot_ws.dart';

class RobotRepository {
  RobotRepository({
    required RobotApi api,
    required RobotWsClient ws,
  })  : _api = api,
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
            name: (zone['settings'] as Map<String, dynamic>?)?['name'] as String? ??
                zone['name'] as String? ??
                'Zone',
            points: mapPointsFromDynamic(zone['polygon']),
            priority: (zone['settings'] as Map<String, dynamic>?)?['priority'] as int? ?? 1,
            mowingDirection: (zone['settings'] as Map<String, dynamic>?)?['mowingDirection'] as String? ??
                'Automatisch',
            mowingProfile: (zone['settings'] as Map<String, dynamic>?)?['mowingProfile'] as String? ??
                'Standard',
          ),
        )
        .where((zone) => zone.points.isNotEmpty)
        .toList(growable: false);
  }

  Map<String, dynamic> encodeMapGeometry(MapGeometry geometry) {
    final zones = geometry.zones.asMap().entries.map((entry) {
      final index = entry.key;
      final zone = entry.value;
      return <String, dynamic>{
        'id': zone.id,
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
    }).toList(growable: false);

    return <String, dynamic>{
      'perimeter': encodeMapPoints(geometry.perimeter),
      'dock': encodeMapPoints(geometry.dock),
      'exclusions': geometry.noGoZones.map(encodeMapPoints).toList(growable: false),
      'zones': zones,
      'mow': const <dynamic>[],
    };
  }
}
