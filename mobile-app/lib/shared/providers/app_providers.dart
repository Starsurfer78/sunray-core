import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/discovery/discovery_repository.dart';
import '../../data/discovery/discovery_service.dart';
import '../../data/local/robot_storage.dart';
import '../../data/local/app_state_storage.dart';
import '../../data/missions/mission_repository.dart';
import '../../data/robot/robot_api.dart';
import '../../data/robot/robot_connection_controller.dart';
import '../../data/robot/robot_repository.dart';
import '../../data/robot/robot_ws.dart';
import '../../data/updates/ota_repository.dart';
import '../../domain/map/map_geometry.dart';
import '../../domain/mission/mission.dart';
import '../../features/map_editor/logic/map_editor_controller.dart';
import '../../features/map_editor/models/map_editor_state.dart';
import '../../domain/robot/discovered_robot.dart';
import '../../domain/robot/saved_robot.dart';
import '../../domain/robot/robot_status.dart';
import '../../domain/update/app_release.dart';
import '../../domain/update/ota_install_state.dart';

final discoveryServiceProvider = Provider<DiscoveryService>((ref) {
  return DiscoveryService();
});

final discoveryRepositoryProvider = Provider<DiscoveryRepository>((ref) {
  return DiscoveryRepository(ref.watch(discoveryServiceProvider));
});

final discoveredRobotsStreamProvider =
    StreamProvider<List<DiscoveredRobot>>((ref) async* {
  yield* ref.watch(discoveryRepositoryProvider).watchRobots();
});

final robotStorageProvider = Provider<RobotStorage>((ref) {
  return RobotStorage();
});

final appStateStorageProvider = Provider<AppStateStorage>((ref) {
  return AppStateStorage();
});

final robotApiProvider = Provider<RobotApi>((ref) {
  return RobotApi();
});

final robotWsProvider = Provider<RobotWsClient>((ref) {
  return RobotWsClient();
});

final robotRepositoryProvider = Provider<RobotRepository>((ref) {
  return RobotRepository(
    api: ref.watch(robotApiProvider),
    ws: ref.watch(robotWsProvider),
  );
});

final missionRepositoryProvider = Provider<MissionRepository>((ref) {
  return MissionRepository(api: ref.watch(robotApiProvider));
});

final robotConnectionControllerProvider =
    Provider<RobotConnectionController>((ref) {
  final controller = RobotConnectionController(
    ref: ref,
    repository: ref.watch(robotRepositoryProvider),
    ws: ref.watch(robotWsProvider),
  );
  ref.onDispose(controller.disconnect);
  return controller;
});

final savedRobotsProvider = FutureProvider<List<SavedRobot>>((ref) async {
  return ref.watch(robotStorageProvider).loadSavedRobots();
});

final rememberedRobotProvider = FutureProvider<SavedRobot?>((ref) async {
  return ref.watch(robotStorageProvider).loadActiveRobot();
});

final activeRobotProvider = StateProvider<SavedRobot?>((ref) {
  return null;
});

final otaRepositoryProvider = Provider<OtaRepository>((ref) {
  return OtaRepository();
});

final connectionStateProvider = StateProvider<RobotStatus>((ref) {
  return const RobotStatus(
    connectionState: ConnectionStateKind.discovering,
  );
});

final mapGeometryProvider = StateProvider<MapGeometry>((ref) {
  return const MapGeometry(
    hasPerimeter: false,
    hasDock: false,
    zoneCount: 0,
    noGoCount: 0,
  );
});

final mapEditorControllerProvider = Provider<MapEditorController>((ref) {
  return const MapEditorController();
});

final mapEditorStateProvider = StateProvider<MapEditorState>((ref) {
  return const MapEditorState();
});

final missionsProvider = StateProvider<List<Mission>>((ref) {
  return const <Mission>[
    Mission(
      id: 'mission-1',
      name: 'Wochenmaehung',
      zoneIds: <String>['zone-nord'],
      zoneNames: <String>['Zone Nord'],
      scheduleLabel: 'Mo 09:00',
    ),
  ];
});

final selectedMissionIdProvider = StateProvider<String?>((ref) {
  return 'mission-1';
});

final missionsLoadingProvider = StateProvider<bool>((ref) => false);

final plannerPreviewPointsProvider = StateProvider<List<MapPoint>>((ref) {
  return const <MapPoint>[];
});

final plannerPreviewLoadingProvider = StateProvider<bool>((ref) => false);

final latestAppReleaseProvider = FutureProvider<AppRelease?>((ref) async {
  return ref.watch(otaRepositoryProvider).fetchLatestAppRelease();
});

final currentAppVersionProvider = FutureProvider<String>((ref) async {
  return ref.watch(otaRepositoryProvider).currentInstalledVersion();
});

final otaInstallStateProvider = StateProvider<OtaInstallState>((ref) {
  return const OtaInstallState();
});
