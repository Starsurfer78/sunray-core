import 'dart:convert';
import 'dart:io';

import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

import '../../domain/map/map_geometry.dart';
import '../../domain/mission/mission.dart';

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
    if (raw is! Map<String, dynamic>) return null;
    return _decodeGeometry(raw);
  }

  Future<void> saveMissions(List<Mission> missions) async {
    final state = await _readState();
    state['missions'] = missions.map((mission) => mission.toJson()).toList(growable: false);
    await _writeState(state);
  }

  Future<List<Mission>> loadMissions() async {
    final state = await _readState();
    final raw = state['missions'];
    if (raw is! List) return const <Mission>[];
    return raw
        .whereType<Map<String, dynamic>>()
        .map(_decodeMission)
        .toList(growable: false);
  }

  Future<Map<String, dynamic>> _readState() async {
    try {
      final file = await _stateFile();
      if (!await file.exists()) return <String, dynamic>{};
      final raw = await file.readAsString();
      final decoded = jsonDecode(raw);
      if (decoded is Map<String, dynamic>) return decoded;
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

  Mission _decodeMission(Map<String, dynamic> raw) {
    // scheduleDays: new structured field; fall back to old scheduleLabel parse.
    List<bool> scheduleDays;
    final rawDays = raw['scheduleDays'];
    if (rawDays is List && rawDays.length == 7) {
      scheduleDays = rawDays.map((e) => e == true).toList(growable: false);
    } else {
      scheduleDays = const <bool>[false, false, false, false, false, false, false];
    }
    return Mission(
      id: raw['id'] as String? ?? '',
      name: raw['name'] as String? ?? 'Mission',
      zoneIds: (raw['zoneIds'] as List<dynamic>? ?? const <dynamic>[]).whereType<String>().toList(growable: false),
      zoneNames: (raw['zoneNames'] as List<dynamic>? ?? const <dynamic>[]).whereType<String>().toList(growable: false),
      scheduleDays: scheduleDays,
      scheduleHour: raw['scheduleHour'] as int?,
      scheduleMinute: raw['scheduleMinute'] as int?,
      scheduleLabel: raw['scheduleLabel'] as String?,
      isRecurring: raw['isRecurring'] as bool? ?? false,
      onlyWhenDry: raw['onlyWhenDry'] as bool? ?? true,
      requiresHighBattery: raw['requiresHighBattery'] as bool? ?? false,
      pattern: raw['pattern'] as String? ?? 'Streifen',
    );
  }
}
