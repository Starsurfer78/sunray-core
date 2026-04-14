import '../../domain/robot/discovered_robot.dart';
import 'discovery_service.dart';

class DiscoveryRepository {
  DiscoveryRepository(this._service);

  final DiscoveryService _service;

  Stream<List<DiscoveredRobot>> watchRobots() async* {
    final robots = <DiscoveredRobot>[];

    await for (final robot in _service.discoverRobots()) {
      final existingIndex = robots.indexWhere((entry) => entry.id == robot.id);
      if (existingIndex >= 0) {
        robots[existingIndex] = robot;
      } else {
        robots.add(robot);
      }
      yield List<DiscoveredRobot>.unmodifiable(robots);
    }
  }
}