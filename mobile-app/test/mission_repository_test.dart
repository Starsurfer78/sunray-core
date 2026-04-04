import 'package:flutter_test/flutter_test.dart';
import 'package:sunray_mobile/data/missions/mission_repository.dart';
import 'package:sunray_mobile/data/robot/robot_api.dart';

void main() {
  final repository = MissionRepository(api: RobotApi());

  test('parses mission payload from backend shape', () {
    final mission = repository.parseMission(<String, dynamic>{
      'id': 'mission-1',
      'name': 'Wochenmaehung',
      'zoneIds': <String>['zone-a', 'zone-b'],
      'zoneNames': <String>['Nord', 'Sued'],
      'schedule': <String, dynamic>{
        'days': <String>['Mo', 'Mi', 'Fr'],
        'time': '09:00',
      },
      'isRecurring': true,
      'onlyWhenDry': true,
      'requiresHighBattery': false,
      'pattern': 'stripe',
    });

    expect(mission.id, 'mission-1');
    expect(mission.zoneIds, <String>['zone-a', 'zone-b']);
    expect(mission.zoneNames, <String>['Nord', 'Sued']);
    expect(mission.scheduleLabel, 'Mo Mi Fr 09:00');
    expect(mission.isRecurring, isTrue);
    expect(mission.pattern, 'stripe');
  });
}
