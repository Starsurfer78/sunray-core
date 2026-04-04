import '../../domain/mission/mission.dart';
import '../robot/robot_api.dart';

class MissionRepository {
  MissionRepository({
    required RobotApi api,
  }) : _api = api;

  final RobotApi _api;

  Future<List<Mission>> fetchMissions({
    required String host,
    required int port,
  }) async {
    final raw = await _api.getMissions(host: host, port: port);
    return raw
        .whereType<Map<String, dynamic>>()
        .map(parseMission)
        .where((mission) => mission.id.isNotEmpty)
        .toList(growable: false);
  }

  Future<void> saveMission({
    required String host,
    required int port,
    required Mission mission,
  }) async {
    try {
      await _api.createMission(
        host: host,
        port: port,
        payload: mission.toJson(),
      );
    } catch (_) {
      await _api.updateMission(
        host: host,
        port: port,
        missionId: mission.id,
        payload: mission.toJson(),
      );
    }
  }

  Future<void> deleteMission({
    required String host,
    required int port,
    required String missionId,
  }) {
    return _api.deleteMission(
      host: host,
      port: port,
      missionId: missionId,
    );
  }

  Mission parseMission(Map<String, dynamic> raw) {
    final zoneIds = (raw['zoneIds'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<String>()
        .toList(growable: false);

    final zoneNames = (raw['zoneNames'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<String>()
        .toList(growable: false);

    final schedule = raw['schedule'];
    final scheduleLabel = raw['scheduleLabel'] as String? ??
        (schedule is Map<String, dynamic>
            ? _scheduleLabelFromMap(schedule)
            : null);

    return Mission(
      id: raw['id'] as String? ?? '',
      name: raw['name'] as String? ?? 'Mission',
      zoneIds: zoneIds,
      zoneNames: zoneNames,
      scheduleLabel: scheduleLabel,
      isRecurring: raw['isRecurring'] as bool? ?? false,
      onlyWhenDry: raw['onlyWhenDry'] as bool? ?? true,
      requiresHighBattery: raw['requiresHighBattery'] as bool? ?? false,
      pattern: (raw['pattern'] as String?) ?? 'Streifen',
    );
  }

  String? _scheduleLabelFromMap(Map<String, dynamic> raw) {
    final time = raw['time'] as String?;
    final days = (raw['days'] as List<dynamic>? ?? const <dynamic>[])
        .whereType<String>()
        .toList(growable: false);
    if (days.isEmpty && (time == null || time.isEmpty)) return null;
    if (days.isEmpty) return time;
    return '${days.join(' ')} ${time ?? ''}'.trim();
  }
}
