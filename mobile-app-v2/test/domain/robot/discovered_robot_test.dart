import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_app_v2/domain/robot/discovered_robot.dart';

void main() {
  group('DiscoveredRobot', () {
    test('supports value equality', () {
      final robot1 = DiscoveredRobot(
        id: '1',
        name: 'Sunray',
        ip: '192.168.1.100',
        port: 80,
      );
      final robot2 = DiscoveredRobot(
        id: '1',
        name: 'Sunray',
        ip: '192.168.1.100',
        port: 80,
      );

      expect(robot1.id, equals(robot2.id));
      expect(robot1.name, equals(robot2.name));
      expect(robot1.ip, equals(robot2.ip));
    });
  });
}
