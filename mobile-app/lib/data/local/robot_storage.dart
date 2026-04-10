import 'dart:convert';
import 'dart:io';

import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

import '../../core/config/app_constants.dart';
import '../../domain/robot/saved_robot.dart';

class RobotStorage {
  static const String _stateFileName = 'sunray_robot_state.json';

  Future<List<SavedRobot>> loadSavedRobots() async {
    final state = await _readState();
    final robotsJson = state['robots'];
    if (robotsJson is! List<dynamic>) return const <SavedRobot>[];

    return robotsJson
        .whereType<Map<String, dynamic>>()
        .map(_savedRobotFromJson)
        .where((r) => r.id.isNotEmpty && r.lastHost.isNotEmpty)
        .toList(growable: false);
  }

  Future<SavedRobot?> loadActiveRobot() async {
    final state = await _readState();
    final activeRobotId = state['activeRobotId'];
    if (activeRobotId is! String || activeRobotId.isEmpty) return null;

    final robots = await loadSavedRobots();
    for (final robot in robots) {
      if (robot.id == activeRobotId) return robot;
    }
    return null;
  }

  Future<void> saveConnectedRobot(SavedRobot robot) async {
    final robots = await loadSavedRobots();
    final updated = <SavedRobot>[
      robot,
      ...robots.where((entry) => entry.id != robot.id),
    ];

    await _writeState(<String, dynamic>{
      'activeRobotId': robot.id,
      'robots': updated.map(_savedRobotToJson).toList(growable: false),
    });
  }

  Future<void> clearActiveRobot() async {
    final robots = await loadSavedRobots();
    await _writeState(<String, dynamic>{
      'activeRobotId': null,
      'robots': robots.map(_savedRobotToJson).toList(growable: false),
    });
  }

  Future<void> forgetRobot(String robotId) async {
    final state = await _readState();
    final robots = await loadSavedRobots();
    final updated =
        robots.where((entry) => entry.id != robotId).toList(growable: false);
    final activeRobotId =
        state['activeRobotId'] == robotId ? null : state['activeRobotId'];

    await _writeState(<String, dynamic>{
      'activeRobotId': activeRobotId,
      'robots': updated.map(_savedRobotToJson).toList(growable: false),
    });
  }

  Future<Map<String, dynamic>> _readState() async {
    try {
      final file = await _stateFile();
      if (!await file.exists()) {
        return <String, dynamic>{
          'activeRobotId': null,
          'robots': <Map<String, dynamic>>[],
        };
      }

      final raw = await file.readAsString();
      final decoded = jsonDecode(raw);
      if (decoded is Map<String, dynamic>) return decoded;
    } catch (_) {
      // Keep startup resilient if the local file is missing or malformed.
    }

    return <String, dynamic>{
      'activeRobotId': null,
      'robots': <Map<String, dynamic>>[],
    };
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

  SavedRobot _savedRobotFromJson(Map<String, dynamic> json) {
    return SavedRobot(
      id: json['id'] as String? ?? '',
      name: json['name'] as String? ?? 'Alfred',
      lastHost: json['lastHost'] as String? ?? '',
      port: json['port'] as int? ?? AppConstants.defaultRobotPort,
      lastSeen: DateTime.tryParse(json['lastSeen'] as String? ?? '') ??
          DateTime.fromMillisecondsSinceEpoch(0),
    );
  }

  Map<String, dynamic> _savedRobotToJson(SavedRobot robot) {
    return <String, dynamic>{
      'id': robot.id,
      'name': robot.name,
      'lastHost': robot.lastHost,
      'port': robot.port,
      'lastSeen': robot.lastSeen.toIso8601String(),
    };
  }
}
