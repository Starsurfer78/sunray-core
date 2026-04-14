import 'dart:convert';
import 'dart:io';

import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

import '../../domain/map/map_geometry.dart';

class AppStateStorage {
  static const String _stateFileName = 'sunray_app_state.json';

  Future<void> saveMapGeometry(MapGeometry geometry) async {
    final state = await _readState();
    state['mapGeometry'] = _encodeGeometry(geometry);
    await _writeState(state);
  }

  Future<MapGeometry?> loadMapGeometry() async {
    final state = await _readState();
    final raw = state['mapGeometry'];
    if (raw is! Map<String, dynamic>) {
      return null;
    }
    return _decodeGeometry(raw);
  }

  Future<Map<String, dynamic>> _readState() async {
    try {
      final file = await _stateFile();
      if (!await file.exists()) {
        return <String, dynamic>{};
      }
      final raw = await file.readAsString();
      final decoded = jsonDecode(raw);
      if (decoded is Map<String, dynamic>) {
        return decoded;
      }
    } catch (_) {
      // Keep local fallback state resilient.
    }
    return <String, dynamic>{};
  }

  Future<void> _writeState(Map<String, dynamic> state) async {
    final file = await _stateFile();
    await file.parent.create(recursive: true);
    await file.writeAsString(
      const JsonEncoder.withIndent('  ').convert(state),
      flush: true,
    );
  }

  Future<File> _stateFile() async {
    final directory = await getApplicationSupportDirectory();
    return File(p.join(directory.path, _stateFileName));
  }

  Map<String, dynamic> _encodeGeometry(MapGeometry geometry) {
    return <String, dynamic>{
      'perimeter': encodeMapPoints(geometry.perimeter),
      'dock': encodeMapPoints(geometry.dock),
      'exclusions': geometry.noGoZones.map(encodeMapPoints).toList(growable: false),
      'zones': geometry.zones
          .map(
            (zone) => <String, dynamic>{
              'id': zone.id,
              'name': zone.name,
              'polygon': encodeMapPoints(zone.points),
              'priority': zone.priority,
              'mowingDirection': zone.mowingDirection,
              'mowingProfile': zone.mowingProfile,
            },
          )
          .toList(growable: false),
    };
  }

  MapGeometry _decodeGeometry(Map<String, dynamic> raw) {
    final perimeter = mapPointsFromDynamic(raw['perimeter']);
    final dock = mapPointsFromDynamic(raw['dock']);
    final noGoZones = (raw['exclusions'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<List>()
        .map(mapPointsFromDynamic)
        .toList(growable: false);
    final zones = (raw['zones'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<Map<String, dynamic>>()
        .map(
          (zone) => ZoneGeometry(
            id: zone['id'] as String? ?? '',
            name: zone['name'] as String? ?? 'Zone',
            points: mapPointsFromDynamic(zone['polygon']),
            priority: zone['priority'] as int? ?? 1,
            mowingDirection: zone['mowingDirection'] as String? ?? 'Automatisch',
            mowingProfile: zone['mowingProfile'] as String? ?? 'Standard',
          ),
        )
        .toList(growable: false);
    return MapGeometry(
      hasPerimeter: perimeter.length >= 3,
      hasDock: dock.isNotEmpty,
      zoneCount: zones.length,
      noGoCount: noGoZones.length,
      perimeter: perimeter,
      dock: dock,
      noGoZones: noGoZones,
      zones: zones,
    );
  }
}