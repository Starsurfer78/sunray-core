import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/widgets.dart';

import '../core/config/app_constants.dart';
import '../data/local/app_state_storage.dart';
import '../data/local/robot_storage.dart';
import '../data/robot/robot_api.dart';
import '../data/robot/robot_repository.dart';
import '../data/robot/robot_service.dart';
import '../data/robot/robot_ws.dart';
import '../data/updates/ota_repository.dart';
import '../domain/map/map_geometry.dart';
import '../domain/map/stored_map.dart';
import '../domain/mission/mission.dart';
import '../domain/robot/robot_status.dart';
import '../domain/robot/saved_robot.dart';
import '../domain/update/app_release.dart';
import '../domain/update/ota_install_state.dart';
import '../features/map/domain/map_editor_controller.dart';
import '../features/map/domain/map_editor_state.dart';

class AppScope extends InheritedNotifier<AppController> {
  const AppScope({
    required AppController controller,
    required super.child,
    super.key,
  }) : super(notifier: controller);

  static AppController watch(BuildContext context) {
    final scope = context.dependOnInheritedWidgetOfExactType<AppScope>();
    assert(scope != null, 'AppScope is missing in the widget tree.');
    return scope!.notifier!;
  }

  static AppController read(BuildContext context) {
    final element = context.getElementForInheritedWidgetOfExactType<AppScope>();
    final scope = element?.widget as AppScope?;
    assert(scope != null, 'AppScope is missing in the widget tree.');
    return scope!.notifier!;
  }
}

enum RobotMode { charging, parking, mowing, delayed }

enum DashboardStage {
  noMap,
  mapNoZones,
  mapZonesCharging,
  mapZonesParking,
  mapZonesMowing,
  mapZonesDelayed,
}

class MapValidationIssue {
  const MapValidationIssue({
    required this.label,
    required this.message,
    this.blocking = true,
  });

  final String label;
  final String message;
  final bool blocking;
}

class AppController extends ChangeNotifier implements RobotStateSink {
  AppController({bool restoreSession = true}) {
    _stateAdapter.controller = this;
    robotRepository = RobotRepository(api: RobotApi(), ws: RobotWsClient());
    robotService = RobotService(
      stateSink: _stateAdapter,
      repository: robotRepository,
    );
    if (restoreSession) {
      Future<void>.microtask(_restoreSavedRobots);
    } else {
      isRestoringSession = false;
    }
  }

  static const int maxMapAreaSquareMeters = 5000;

  final _RobotStateAdapter _stateAdapter = _RobotStateAdapter();
  late final RobotRepository robotRepository;
  late final RobotService robotService;
  final OtaRepository otaRepository = OtaRepository();
  final AppStateStorage appStateStorage = AppStateStorage();
  final MapEditorController mapEditorController = const MapEditorController();
  final RobotStorage robotStorage = RobotStorage();

  bool hasKnownRobot = false;
  bool isRestoringSession = true;
  String robotName = 'Alfred';
  SavedRobot? connectedRobot;
  List<SavedRobot> savedRobots = const <SavedRobot>[];
  RobotStatus connectionStatus = const RobotStatus(
    connectionState: ConnectionStateKind.disconnected,
  );
  MapGeometry mapGeometry = const MapGeometry();
  MapEditorState mapEditorState = const MapEditorState();
  List<Mission> backendMissions = const <Mission>[];

  List<StoredMap> storedMaps = const <StoredMap>[];
  String activeMapId = '';

  bool hasMap = false;
  int zoneCount = 0;
  int noGoCount = 0;
  int channelCount = 0;
  int perimeterPointCount = 0;
  int mapAreaSquareMeters = 0;
  int weeklyAreaSquareMeters = 0;
  int batteryPercent = 0;
  int unreadNotifications = 0;
  bool onlyWhenDry = true;
  bool requiresHighBattery = true;
  String missionName = 'Keine Mission';
  String missionZoneLabel = 'Gesamte Karte';
  String nextMissionLabel = 'Nur manuell';
  RobotMode robotMode = RobotMode.charging;
  List<MapPoint> previewRoutePoints = const <MapPoint>[];
  String? previewError;
  bool isLoadingPreview = false;
  String? installedAppVersion;
  AppRelease? latestAppRelease;
  bool isLoadingAppUpdateInfo = false;
  String? appUpdateInfoError;
  OtaInstallState appOtaInstallState = const OtaInstallState();
  int _missionCounter = 1;
  List<AppMission> _missions = const <AppMission>[];
  String? _lastNotifiedMessage;
  String? _lastNotifiedError;

  final List<_AppSnapshot> _history = <_AppSnapshot>[];
  final List<MapGeometry> _mapUndoStack = <MapGeometry>[];
  final List<MapGeometry> _mapRedoStack = <MapGeometry>[];
  MapGeometry? _recordingBaseline;
  MapGeometry? _dragBaseline;
  bool _isDraggingMapPoint = false;

  bool get hasZones => zoneCount > 0;
  bool get canUndo => _history.isNotEmpty;
  String get activeMapName {
    if (activeMapId.isEmpty) return '';
    try {
      return storedMaps.firstWhere((m) => m.id == activeMapId).name;
    } catch (_) {
      return '';
    }
  }
  bool get canUndoMapEdit => _mapUndoStack.isNotEmpty;
  bool get canRedoMapEdit => _mapRedoStack.isNotEmpty;
  SavedRobot? get lastConnectedRobot =>
      savedRobots.isEmpty ? null : savedRobots.first;
  List<MapPoint> get activeMapPoints =>
      mapEditorController.activePoints(mapGeometry, mapEditorState);
  bool get hasPerimeterReady => mapGeometry.perimeter.length >= 3;
  bool get hasDockReady => mapGeometry.dock.length >= 2;
  bool get hasValidNoGoZones =>
      mapGeometry.noGoZones.every((zone) => zone.isEmpty || zone.length >= 3);
  bool get hasValidZoneShapes => mapGeometry.zones.every(
    (zone) => zone.points.isEmpty || zone.points.length >= 3,
  );
  bool get hasBaseMapReady =>
      hasPerimeterReady && hasDockReady && hasValidNoGoZones;
  bool get canSaveMap => hasBaseMapReady && hasValidZoneShapes;
  bool get canLoadPlannerPreview =>
      connectedRobot != null &&
      hasBaseMapReady &&
      mapGeometry.zones.isNotEmpty &&
      hasValidZoneShapes;
  bool get hasLoadedAppUpdateInfo =>
      installedAppVersion != null ||
      latestAppRelease != null ||
      appUpdateInfoError != null;
  bool get isAppUpdateAvailable =>
      latestAppRelease != null &&
      installedAppVersion != null &&
      latestAppRelease!.hasApkAsset &&
      latestAppRelease!.isNewerThan(installedAppVersion!);
  String get appOtaSummary {
    if (isLoadingAppUpdateInfo) {
      return 'Prüfe GitHub Releases...';
    }
    if (appUpdateInfoError != null) {
      return 'Release-Check fehlgeschlagen';
    }
    if (isAppUpdateAvailable) {
      return 'Neue APK über GitHub Releases verfügbar';
    }
    if (latestAppRelease != null && installedAppVersion != null) {
      return 'App ist aktuell';
    }
    return 'GitHub Releases prüfen und APK installieren';
  }

  List<MapValidationIssue> get mapValidationIssues => <MapValidationIssue>[
    if (!hasPerimeterReady)
      const MapValidationIssue(
        label: 'Perimeter',
        message: 'Der Perimeter braucht mindestens drei Punkte.',
      ),
    if (!hasDockReady)
      const MapValidationIssue(
        label: 'Dock',
        message: 'Das Dock braucht mindestens zwei Punkte.',
      ),
    for (var index = 0; index < mapGeometry.noGoZones.length; index += 1)
      if (mapGeometry.noGoZones[index].isNotEmpty &&
          mapGeometry.noGoZones[index].length < 3)
        MapValidationIssue(
          label: 'No-Go ${index + 1}',
          message:
              'No-Go-Bereiche müssen mit mindestens drei Punkten geschlossen werden.',
        ),
    for (final zone in mapGeometry.zones)
      if (zone.points.isNotEmpty && zone.points.length < 3)
        MapValidationIssue(
          label: zone.name,
          message: 'Zonen brauchen mindestens drei Punkte.',
        ),
    if (mapGeometry.zones.isEmpty)
      const MapValidationIssue(
        label: 'Zonen',
        message:
            'Noch keine Zone angelegt. Für die Planner-Vorschau ist mindestens eine Zone nötig.',
        blocking: false,
      ),
  ];
  bool get hasBlockingMapValidationIssues =>
      mapValidationIssues.any((issue) => issue.blocking);
  List<AppMission> get missions => List<AppMission>.unmodifiable(_missions);

  void beginMapCreation() {
    mapGeometry = const MapGeometry();
    mapEditorState = const MapEditorState(
      mode: MapEditorMode.record,
      activeObject: EditableMapObjectType.perimeter,
      setupStage: MapSetupStage.perimeter,
    );
    _mapUndoStack.clear();
    _mapRedoStack.clear();
    _recordingBaseline = const MapGeometry();
    _dragBaseline = null;
    _isDraggingMapPoint = false;
    previewRoutePoints = const <MapPoint>[];
    previewError = null;
    isLoadingPreview = false;
    hasMap = false;
    zoneCount = 0;
    noGoCount = 0;
    channelCount = 0;
    perimeterPointCount = 0;
    mapAreaSquareMeters = 0;
    notifyListeners();
  }
  DashboardStage get dashboardStage {
    if (!hasMap) {
      return DashboardStage.noMap;
    }
    if (!hasZones) {
      return DashboardStage.mapNoZones;
    }
    return switch (robotMode) {
      RobotMode.charging => DashboardStage.mapZonesCharging,
      RobotMode.parking => DashboardStage.mapZonesParking,
      RobotMode.mowing => DashboardStage.mapZonesMowing,
      RobotMode.delayed => DashboardStage.mapZonesDelayed,
    };
  }

  String get topStatus {
    return switch (dashboardStage) {
      DashboardStage.noMap => 'Noch keine Karte',
      DashboardStage.mapNoZones => 'Basiskarte bereit',
      DashboardStage.mapZonesCharging => 'In Station',
      DashboardStage.mapZonesParking => 'Steht auf der Karte',
      DashboardStage.mapZonesMowing => 'Mäht gerade',
      DashboardStage.mapZonesDelayed => 'Mähen wegen Regen verzögert',
    };
  }

  String get statusLabel {
    return switch (dashboardStage) {
      DashboardStage.noMap => 'Verbunden',
      DashboardStage.mapNoZones => 'Karte bereit',
      DashboardStage.mapZonesCharging => 'Lädt',
      DashboardStage.mapZonesParking => 'Geparkt',
      DashboardStage.mapZonesMowing => 'Mäht',
      DashboardStage.mapZonesDelayed => 'Mähen verzögert',
    };
  }

  String get nextActionValue {
    if (!hasMap) {
      return 'Karte anlegen';
    }
    if (!hasZones) {
      return 'Jetzt bereit';
    }
    if (robotMode == RobotMode.delayed) {
      return 'Regen';
    }
    if (robotMode == RobotMode.mowing) {
      return '20:30';
    }
    return '17:00';
  }

  String get nextActionLabel {
    if (!hasMap) {
      return 'Nächster Schritt';
    }
    if (!hasZones) {
      return 'Gesamte Karte';
    }
    if (robotMode == RobotMode.delayed) {
      return 'Verzögerung';
    }
    if (robotMode == RobotMode.mowing) {
      return 'Nächster Lauf';
    }
    return 'Nächste Mission';
  }

  String? get missionSubtitle {
    if (_missions.isEmpty && !hasZones) {
      return null;
    }
    return switch (robotMode) {
      RobotMode.charging => '$missionZoneLabel • $nextMissionLabel',
      RobotMode.parking => '$missionZoneLabel • Manueller Start',
      RobotMode.mowing => '$missionZoneLabel • 64 % abgeschlossen',
      RobotMode.delayed => '$missionZoneLabel • Warten auf trockenes Wetter',
    };
  }

  String get primaryDashboardLabel {
    return switch (dashboardStage) {
      DashboardStage.noMap => 'Karte anlegen',
      DashboardStage.mapNoZones => 'Jetzt mähen',
      DashboardStage.mapZonesCharging => 'Jetzt mähen',
      DashboardStage.mapZonesParking => 'Mähen',
      DashboardStage.mapZonesMowing => 'Pause',
      DashboardStage.mapZonesDelayed => 'Jetzt mähen',
    };
  }

  String? get secondaryDashboardLabel {
    return switch (dashboardStage) {
      DashboardStage.noMap => null,
      DashboardStage.mapNoZones => null,
      DashboardStage.mapZonesCharging => 'Mission',
      DashboardStage.mapZonesParking => 'Station',
      DashboardStage.mapZonesMowing => 'Station',
      DashboardStage.mapZonesDelayed => 'Mission',
    };
  }

  @override
  void dispose() {
    _stateAdapter.controller = null;
    robotService.dispose();
    super.dispose();
  }

  Future<RobotStatus> connectRobot({SavedRobot? robot}) {
    final target =
        robot ??
        SavedRobot(
          id: '${AppConstants.defaultRobotHost}:${AppConstants.defaultRobotPort}',
          name: robotName,
          lastHost: AppConstants.defaultRobotHost,
          port: AppConstants.defaultRobotPort,
          lastSeen: DateTime.now(),
        );
    return _connectAndRemember(target);
  }

  Future<void> forgetRobot() async {
    await robotService.disconnect(clearKnownRobot: true);
    if (connectedRobot != null) {
      await robotStorage.forgetRobot(connectedRobot!.id);
    } else {
      await robotStorage.clearActiveRobot();
    }
    hasKnownRobot = false;
    connectedRobot = null;
    savedRobots = await robotStorage.loadSavedRobots();
    connectionStatus = const RobotStatus(
      connectionState: ConnectionStateKind.disconnected,
    );
    mapGeometry = const MapGeometry();
    mapEditorState = const MapEditorState();
    _mapUndoStack.clear();
    _mapRedoStack.clear();
    _recordingBaseline = null;
    _dragBaseline = null;
    _isDraggingMapPoint = false;
    previewRoutePoints = const <MapPoint>[];
    previewError = null;
    isLoadingPreview = false;
    backendMissions = const <Mission>[];
    _history.clear();
    hasMap = false;
    zoneCount = 0;
    noGoCount = 0;
    channelCount = 0;
    perimeterPointCount = 0;
    mapAreaSquareMeters = 0;
    robotMode = RobotMode.charging;
    _missionCounter = 1;
    _missions = const <AppMission>[];
    missionName = 'Keine Mission';
    missionZoneLabel = 'Gesamte Karte';
    nextMissionLabel = 'Nur manuell';
    notifyListeners();
  }

  @override
  void applyConnectedRobot(SavedRobot? robot, {bool notify = true}) {
    connectedRobot = robot;
    hasKnownRobot = robot != null;
    if (robot != null) {
      robotName = robot.name;
    }
    if (notify) {
      notifyListeners();
    }
  }

  Future<void> forgetSavedRobot(String robotId) async {
    await robotStorage.forgetRobot(robotId);
    savedRobots = await robotStorage.loadSavedRobots();
    if (connectedRobot?.id == robotId) {
      connectedRobot = null;
      hasKnownRobot = false;
      connectionStatus = const RobotStatus(
        connectionState: ConnectionStateKind.disconnected,
      );
    }
    notifyListeners();
  }

  @override
  void applyConnectionStatus(RobotStatus status, {bool notify = true}) {
    connectionStatus =
        status.connectionState == ConnectionStateKind.connected
        ? connectionStatus.copyWith(
            connectionState: status.connectionState,
            robotName: status.robotName,
            batteryPercent: status.batteryPercent,
            rtkState: status.rtkState,
            statePhase: status.statePhase,
            x: status.x,
            y: status.y,
            heading: status.heading,
            gpsLat: status.gpsLat,
            gpsLon: status.gpsLon,
            gpsNumSv: status.gpsNumSv,
            gpsDgpsAgeMs: status.gpsDgpsAgeMs,
            piVersion: status.piVersion,
            lastError: status.lastError,
            batteryVoltage: status.batteryVoltage,
            chargerConnected: status.chargerConnected,
            mcuConnected: status.mcuConnected,
            mcuVersion: status.mcuVersion,
            runtimeHealth: status.runtimeHealth,
            uptimeSeconds: status.uptimeSeconds,
            mowFaultActive: status.mowFaultActive,
            uiMessage: status.uiMessage,
            uiSeverity: status.uiSeverity,
            mowDistanceM: status.mowDistanceM,
            mowDurationSec: status.mowDurationSec,
            wifiRssiDbm: status.wifiRssiDbm,
            bluetoothConnected: status.bluetoothConnected,
          )
        : status;
    final incomingMessage = status.uiMessage;
    if (incomingMessage != null &&
        incomingMessage.isNotEmpty &&
        incomingMessage != _lastNotifiedMessage) {
      unreadNotifications += 1;
      _lastNotifiedMessage = incomingMessage;
    }
    final incomingError = status.lastError;
    if (incomingError != null &&
        incomingError.isNotEmpty &&
        incomingError != _lastNotifiedError) {
      unreadNotifications += 1;
      _lastNotifiedError = incomingError;
    }
    if (status.robotName != null && status.robotName!.trim().isNotEmpty) {
      robotName = status.robotName!.trim();
    }
    if (status.batteryPercent != null) {
      batteryPercent = status.batteryPercent!;
    }
    robotMode = _robotModeFromStatus(status);
    if (notify) {
      notifyListeners();
    }
  }

  @override
  void applyMapGeometry(MapGeometry geometry, {bool notify = true}) {
    mapGeometry = geometry;
    hasMap = geometry.hasPerimeter || geometry.perimeter.length >= 3;
    zoneCount = geometry.zoneCount;
    noGoCount = geometry.noGoCount;
    perimeterPointCount = geometry.perimeter.length;
    mapAreaSquareMeters = _polygonAreaSquareMeters(geometry.perimeter);
    if (notify) {
      notifyListeners();
    }
  }

  void setMapEditorMode(MapEditorMode mode) {
    if (mode == MapEditorMode.record) {
      _recordingBaseline ??= mapGeometry;
    }
    if (mode == MapEditorMode.view) {
      _recordingBaseline = null;
    }
    mapEditorState = mapEditorState.copyWith(
      mode: mode,
      showToolPanel: mode == MapEditorMode.view
          ? mapEditorState.showToolPanel
          : true,
      clearSelectedPoint: mode == MapEditorMode.view,
    );
    notifyListeners();
  }

  void setMapToolPanelVisible(bool visible) {
    mapEditorState = mapEditorState.copyWith(showToolPanel: visible);
    notifyListeners();
  }

  void setMapSetupStage(MapSetupStage stage) {
    var nextGeometry = mapGeometry;
    int? nextActiveNoGoIndex = mapEditorState.activeNoGoIndex;

    if (stage == MapSetupStage.noGo && nextGeometry.noGoZones.isEmpty) {
      nextGeometry = mapEditorController.addNoGo(nextGeometry);
      nextActiveNoGoIndex = nextGeometry.noGoZones.length - 1;
      applyMapGeometry(nextGeometry, notify: false);
    }

    final nextMode = switch (stage) {
      MapSetupStage.perimeter || MapSetupStage.noGo || MapSetupStage.dock =>
        MapEditorMode.record,
      MapSetupStage.validate || MapSetupStage.save => MapEditorMode.view,
    };
    if (nextMode == MapEditorMode.record) {
      _recordingBaseline ??= mapGeometry;
    }

    mapEditorState = mapEditorState.copyWith(
      setupStage: stage,
      activeObject: switch (stage) {
        MapSetupStage.perimeter => EditableMapObjectType.perimeter,
        MapSetupStage.noGo => EditableMapObjectType.noGo,
        MapSetupStage.dock => EditableMapObjectType.dock,
        MapSetupStage.validate ||
        MapSetupStage.save => mapEditorState.activeObject,
      },
      activeNoGoIndex: stage == MapSetupStage.noGo
          ? nextActiveNoGoIndex
          : mapEditorState.activeNoGoIndex,
      mode: nextMode,
      showToolPanel: stage == MapSetupStage.save
          ? mapEditorState.showToolPanel
          : true,
      clearSelectedPoint: true,
    );
    notifyListeners();
  }

  void advanceMapSetupStage() {
    final nextStage = switch (mapEditorState.setupStage) {
      MapSetupStage.perimeter =>
        hasPerimeterReady ? MapSetupStage.noGo : MapSetupStage.perimeter,
      MapSetupStage.noGo => MapSetupStage.dock,
      MapSetupStage.dock =>
        hasDockReady ? MapSetupStage.save : MapSetupStage.dock,
      MapSetupStage.validate =>
        hasBlockingMapValidationIssues ? MapSetupStage.validate : MapSetupStage.save,
      MapSetupStage.save => MapSetupStage.save,
    };
    setMapSetupStage(nextStage);
  }

  void retreatMapSetupStage() {
    final previousStage = switch (mapEditorState.setupStage) {
      MapSetupStage.perimeter => MapSetupStage.perimeter,
      MapSetupStage.noGo => MapSetupStage.perimeter,
      MapSetupStage.dock => MapSetupStage.noGo,
      MapSetupStage.validate => MapSetupStage.dock,
      MapSetupStage.save => MapSetupStage.validate,
    };
    setMapSetupStage(previousStage);
  }

  void setActiveZone(String zoneId) {
    mapEditorState = mapEditorState.copyWith(
      activeObject: EditableMapObjectType.zone,
      activeZoneId: zoneId,
      setupStage: hasBaseMapReady
          ? MapSetupStage.save
          : mapEditorState.setupStage,
      mode: mapEditorState.mode == MapEditorMode.view
          ? MapEditorMode.edit
          : mapEditorState.mode,
      clearActiveNoGo: true,
      clearSelectedPoint: true,
    );
    notifyListeners();
  }

  void setActiveMapObject(
    EditableMapObjectType object, {
    String? zoneId,
    int? noGoIndex,
    MapEditorMode? mode,
  }) {
    mapEditorState = mapEditorState.copyWith(
      activeObject: object,
      activeZoneId: object == EditableMapObjectType.zone ? zoneId : null,
      activeNoGoIndex: object == EditableMapObjectType.noGo ? noGoIndex : null,
      setupStage: hasBaseMapReady
          ? MapSetupStage.save
          : switch (object) {
              EditableMapObjectType.perimeter => MapSetupStage.perimeter,
              EditableMapObjectType.noGo => MapSetupStage.noGo,
              EditableMapObjectType.dock => MapSetupStage.dock,
              EditableMapObjectType.zone => mapEditorState.setupStage,
            },
      mode: mode,
      showToolPanel: true,
      clearActiveZone: object != EditableMapObjectType.zone,
      clearActiveNoGo: object != EditableMapObjectType.noGo,
      clearSelectedPoint: true,
    );
    if (mapEditorState.mode == MapEditorMode.record) {
      _recordingBaseline ??= mapGeometry;
    }
    notifyListeners();
  }

  void createMapObject(EditableMapObjectType object) {
    _recordingBaseline = mapGeometry;
    switch (object) {
      case EditableMapObjectType.perimeter:
        mapEditorState = const MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.perimeter,
          setupStage: MapSetupStage.perimeter,
          showToolPanel: true,
        );
      case EditableMapObjectType.dock:
        mapEditorState = const MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.dock,
          setupStage: MapSetupStage.dock,
          showToolPanel: true,
        );
      case EditableMapObjectType.noGo:
        final next = mapEditorController.addNoGo(mapGeometry);
        _setMapGeometry(next, notify: false);
        mapEditorState = MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.noGo,
          setupStage: MapSetupStage.noGo,
          activeNoGoIndex: next.noGoZones.length - 1,
          showToolPanel: true,
        );
      case EditableMapObjectType.zone:
        final next = mapEditorController.addZone(mapGeometry);
        final zoneId = next.zones.last.id;
        _setMapGeometry(next, notify: false);
        mapEditorState = MapEditorState(
          mode: MapEditorMode.record,
          activeObject: EditableMapObjectType.zone,
          setupStage: MapSetupStage.save,
          activeZoneId: zoneId,
          showToolPanel: true,
        );
    }
    notifyListeners();
  }

  void addMapPoint(MapPoint point) {
    if (mapEditorState.mode != MapEditorMode.record) {
      return;
    }
    _recordingBaseline ??= mapGeometry;
    final next = mapEditorController.addPoint(
      mapGeometry,
      mapEditorState,
      point,
    );
    _setMapGeometry(next);
  }

  void insertMapPoint(int insertIndex, MapPoint point) {
    final next = mapEditorController.insertPoint(
      mapGeometry,
      mapEditorState,
      insertIndex,
      point,
    );
    _setMapGeometry(next);
  }

  void selectMapPoint(int index) {
    mapEditorState = mapEditorState.copyWith(selectedPointIndex: index);
    notifyListeners();
  }

  void moveSelectedMapPoint(MapPoint point) {
    final selectedPointIndex = mapEditorState.selectedPointIndex;
    if (selectedPointIndex == null) {
      return;
    }
    _recordingBaseline ??= mapGeometry;
    final next = mapEditorController.movePoint(
      mapGeometry,
      mapEditorState,
      selectedPointIndex,
      point,
    );
    _setMapGeometry(next);
  }

  void startMapPointDrag(int index) {
    mapEditorState = mapEditorState.copyWith(
      mode: MapEditorMode.edit,
      selectedPointIndex: index,
    );
    _recordingBaseline ??= mapGeometry;
    _dragBaseline ??= mapGeometry;
    _isDraggingMapPoint = true;
    notifyListeners();
  }

  void dragMapPoint(int index, MapPoint point) {
    final next = mapEditorController.movePoint(
      mapGeometry,
      mapEditorState,
      index,
      point,
    );
    applyMapGeometry(next, notify: false);
    notifyListeners();
  }

  void finishMapPointDrag() {
    if (!_isDraggingMapPoint) {
      return;
    }
    final baseline = _dragBaseline;
    _dragBaseline = null;
    _isDraggingMapPoint = false;
    if (baseline != null && !_sameGeometry(baseline, mapGeometry)) {
      _mapUndoStack.add(baseline);
      _mapRedoStack.clear();
    }
    notifyListeners();
  }

  void deleteSelectedMapPoint() {
    final selectedPointIndex = mapEditorState.selectedPointIndex;
    if (selectedPointIndex == null) {
      return;
    }
    final next = mapEditorController.deletePoint(
      mapGeometry,
      mapEditorState,
      selectedPointIndex,
    );
    _setMapGeometry(next, notify: false);
    mapEditorState = mapEditorState.copyWith(clearSelectedPoint: true);
    notifyListeners();
  }

  void deleteActiveMapObject() {
    final next = mapEditorController.deleteActiveObject(
      mapGeometry,
      mapEditorState,
    );
    _setMapGeometry(next, notify: false);
    mapEditorState = const MapEditorState();
    _recordingBaseline = null;
    notifyListeners();
  }

  void undoMapEdit() {
    if (_mapUndoStack.isEmpty) {
      return;
    }
    final previous = _mapUndoStack.removeLast();
    _mapRedoStack.add(mapGeometry);
    applyMapGeometry(previous, notify: false);
    mapEditorState = mapEditorState.copyWith(clearSelectedPoint: true);
    notifyListeners();
  }

  void redoMapEdit() {
    if (_mapRedoStack.isEmpty) {
      return;
    }
    final next = _mapRedoStack.removeLast();
    _mapUndoStack.add(mapGeometry);
    applyMapGeometry(next, notify: false);
    mapEditorState = mapEditorState.copyWith(clearSelectedPoint: true);
    notifyListeners();
  }

  void finishMapRecording() {
    _recordingBaseline = null;
    mapEditorState = mapEditorState.copyWith(
      mode: MapEditorMode.view,
      showToolPanel: false,
      setupStage: hasBaseMapReady
          ? MapSetupStage.save
          : switch (mapEditorState.activeObject) {
              EditableMapObjectType.perimeter =>
                hasPerimeterReady ? MapSetupStage.noGo : MapSetupStage.perimeter,
              EditableMapObjectType.noGo => MapSetupStage.noGo,
              EditableMapObjectType.dock =>
                hasDockReady ? MapSetupStage.save : MapSetupStage.dock,
              EditableMapObjectType.zone => MapSetupStage.save,
            },
      clearSelectedPoint: true,
    );
    notifyListeners();
  }

  void deleteLastRecordedMapPoint() {
    if (mapEditorState.mode != MapEditorMode.record) {
      return;
    }
    final points = activeMapPoints;
    if (points.isEmpty) {
      return;
    }
    final next = mapEditorController.deletePoint(
      mapGeometry,
      mapEditorState,
      points.length - 1,
    );
    _setMapGeometry(next);
  }

  void cancelMapRecording() {
    if (_recordingBaseline != null) {
      applyMapGeometry(_recordingBaseline!, notify: false);
    }
    _recordingBaseline = null;
    _dragBaseline = null;
    _isDraggingMapPoint = false;
    _mapUndoStack.clear();
    _mapRedoStack.clear();
    mapEditorState = const MapEditorState();
    notifyListeners();
  }

  Future<String?> saveMapEdits({String? name}) async {
    if (!canSaveMap) {
      final missingParts = <String>[
        if (!hasPerimeterReady) 'Perimeter',
        if (!hasDockReady) 'Docking',
        if (!hasValidNoGoZones) 'gültige No-Go-Bereiche',
        if (!hasValidZoneShapes) 'vollständige Zonen',
      ];
      return 'Speichern ist erst nach gültiger Basiskarte möglich. Es fehlt noch: ${missingParts.join(', ')}.';
    }

    try {
      await appStateStorage.saveMapGeometry(mapGeometry);
      if (connectedRobot != null) {
        final robot = connectedRobot!;
        final resolvedName = name?.trim() ?? '';
        final isNewName =
            resolvedName.isNotEmpty && resolvedName != activeMapName;
        final hasNoActiveMap = activeMapId.isEmpty;

        if (isNewName || hasNoActiveMap) {
          await robotRepository.createNamedMap(
            host: robot.lastHost,
            port: robot.port,
            name: resolvedName.isNotEmpty ? resolvedName : 'Mein Garten',
            geometry: mapGeometry,
          );
        } else {
          await robotRepository.saveMapGeometry(
            host: robot.lastHost,
            port: robot.port,
            geometry: mapGeometry,
          );
        }

        final reloaded = await robotRepository.fetchMapGeometry(
          host: robot.lastHost,
          port: robot.port,
        );
        applyMapGeometry(reloaded, notify: false);

        try {
          final storedResult = await robotRepository.fetchStoredMaps(
            host: robot.lastHost,
            port: robot.port,
          );
          applyStoredMaps(storedResult.activeId, storedResult.maps, notify: false);
        } catch (_) {}
      }
      _recordingBaseline = null;
      _dragBaseline = null;
      _isDraggingMapPoint = false;
      _mapUndoStack.clear();
      _mapRedoStack.clear();
      mapEditorState = mapEditorState.copyWith(
        mode: MapEditorMode.view,
        setupStage: MapSetupStage.save,
        showToolPanel: false,
        clearSelectedPoint: true,
      );
      notifyListeners();
      return null;
    } catch (error) {
      return 'Karte konnte nicht gespeichert werden: $error';
    }
  }

  Future<String?> switchToStoredMap(String mapId) async {
    final robot = connectedRobot;
    if (robot == null) return 'Nicht verbunden';
    try {
      await robotRepository.activateStoredMapById(
        host: robot.lastHost,
        port: robot.port,
        mapId: mapId,
      );
      final reloaded = await robotRepository.fetchMapGeometry(
        host: robot.lastHost,
        port: robot.port,
      );
      applyMapGeometry(reloaded, notify: false);
      final storedResult = await robotRepository.fetchStoredMaps(
        host: robot.lastHost,
        port: robot.port,
      );
      applyStoredMaps(storedResult.activeId, storedResult.maps, notify: false);
      mapEditorState = const MapEditorState();
      _mapUndoStack.clear();
      _mapRedoStack.clear();
      _recordingBaseline = null;
      notifyListeners();
      return null;
    } catch (error) {
      return 'Karte konnte nicht gewechselt werden: $error';
    }
  }

  Future<String?> loadPlannerPreview() async {
    final robot = connectedRobot;
    if (robot == null) {
      previewRoutePoints = const <MapPoint>[];
      previewError = 'Planner-Vorschau braucht eine Verbindung zu Alfred.';
      notifyListeners();
      return previewError;
    }
    if (!hasBaseMapReady || !hasValidZoneShapes) {
      previewRoutePoints = const <MapPoint>[];
      previewError =
          'Planner-Vorschau ist erst mit gültiger Karte und vollständigen Zonen möglich.';
      notifyListeners();
      return previewError;
    }
    if (mapGeometry.zones.isEmpty) {
      previewRoutePoints = const <MapPoint>[];
      previewError = 'Planner-Vorschau braucht mindestens eine Zone.';
      notifyListeners();
      return previewError;
    }

    isLoadingPreview = true;
    previewError = null;
    notifyListeners();

    try {
      previewRoutePoints = await robotRepository.fetchPlannerPreview(
        host: robot.lastHost,
        port: robot.port,
        geometry: mapGeometry,
        zoneIds: mapGeometry.zones
            .map((zone) => zone.id)
            .toList(growable: false),
      );
      previewError = null;
      return null;
    } catch (error) {
      previewRoutePoints = const <MapPoint>[];
      previewError = error.toString().replaceFirst('FormatException: ', '');
      return previewError;
    } finally {
      isLoadingPreview = false;
      notifyListeners();
    }
  }

  void clearPlannerPreview() {
    previewRoutePoints = const <MapPoint>[];
    previewError = null;
    isLoadingPreview = false;
    notifyListeners();
  }

  void updateZoneConfiguration(
    String zoneId, {
    required String name,
    required int priority,
    required String mowingDirection,
    required String mowingProfile,
  }) {
    final nextZones = mapGeometry.zones
        .map(
          (zone) => zone.id == zoneId
              ? zone.copyWith(
                  name: name,
                  priority: priority,
                  mowingDirection: mowingDirection,
                  mowingProfile: mowingProfile,
                )
              : zone,
        )
        .toList(growable: false);
    _setMapGeometry(mapGeometry.copyWith(zones: nextZones), notify: false);
    mapEditorState = mapEditorState.copyWith(
      activeObject: EditableMapObjectType.zone,
      activeZoneId: zoneId,
      setupStage: MapSetupStage.save,
    );
    notifyListeners();
  }

  @override
  void applyMissions(List<Mission> missions, {bool notify = true}) {
    backendMissions = missions;
    _missions = missions
        .map(
          (mission) => AppMission(
            id: mission.id,
            name: mission.name,
            zones: mission.zoneNames.isNotEmpty
                ? mission.zoneNames
                : mission.zoneIds.map((id) {
                    try {
                      return mapGeometry.zones
                          .firstWhere((z) => z.id == id)
                          .name;
                    } catch (_) {
                      return id;
                    }
                  }).toList(growable: false),
            scheduleLabel: mission.effectiveScheduleLabel,
            statusLabel: mission.zoneIds.isEmpty ? 'Entwurf' : 'Geplant',
            isEnabled: mission.zoneIds.isNotEmpty,
            onlyWhenDry: mission.onlyWhenDry,
            requiresHighBattery: mission.requiresHighBattery,
            optimizeOrder: true,
          ),
        )
        .toList(growable: false);
    if (_missions.isNotEmpty) {
      final first = _missions.first;
      missionName = first.name;
      missionZoneLabel = first.zones.isEmpty
          ? 'Gesamte Karte'
          : first.zones.join(', ');
      nextMissionLabel = first.scheduleLabel;
      final loadedCounter = _missions
          .map(
            (mission) =>
                int.tryParse(mission.id.replaceFirst('mission-', '')) ?? 0,
          )
          .fold(0, (maxValue, value) => value > maxValue ? value : maxValue);
      _missionCounter = loadedCounter > 0 ? loadedCounter : _missionCounter;
    } else {
      missionName = 'Keine Mission';
      missionZoneLabel = hasZones ? 'Zonen verfügbar' : 'Gesamte Karte';
      nextMissionLabel = 'Nur manuell';
    }
    if (notify) {
      notifyListeners();
    }
  }

  @override
  void applyStoredMaps(
    String activeId,
    List<StoredMap> maps, {
    bool notify = true,
  }) {
    activeMapId = activeId;
    storedMaps = maps;
    if (notify) {
      notifyListeners();
    }
  }

  @override
  void applyTelemetry(RobotStatus status) {
    applyConnectionStatus(status, notify: false);
    notifyListeners();
  }

  bool canCloseActiveMapObject() {
    final activePoints = activeMapPoints;
    return switch (mapEditorState.activeObject) {
      EditableMapObjectType.dock => activePoints.length >= 2,
      EditableMapObjectType.perimeter ||
      EditableMapObjectType.noGo ||
      EditableMapObjectType.zone => activePoints.length >= 3,
    };
  }

  void createMap() {
    _record();
    hasMap = true;
    zoneCount = 0;
    noGoCount = 0;
    channelCount = 0;
    perimeterPointCount = 24;
    mapAreaSquareMeters = 210;
    robotMode = RobotMode.charging;
    notifyListeners();
  }

  void addPerimeter() {
    if (!hasMap) {
      createMap();
      return;
    }
    _record();
    perimeterPointCount += 4;
    mapAreaSquareMeters += 160;
    notifyListeners();
  }

  void addNoGoZone() {
    if (!hasMap) {
      createMap();
    }
    _record();
    noGoCount += 1;
    notifyListeners();
  }

  void addChannel() {
    if (!hasMap) {
      createMap();
    }
    _record();
    channelCount += 1;
    mapAreaSquareMeters += 45;
    notifyListeners();
  }

  void addZone() {
    if (!hasMap) {
      createMap();
    }
    _record();
    zoneCount += 1;
    mapAreaSquareMeters += 120;
    notifyListeners();
  }

  void deletePoint() {
    if (perimeterPointCount <= 4) {
      return;
    }
    _record();
    perimeterPointCount -= 1;
    notifyListeners();
  }

  void deleteSelectedObject() {
    if (noGoCount > 0) {
      _record();
      noGoCount -= 1;
      notifyListeners();
      return;
    }
    if (channelCount > 0) {
      _record();
      channelCount -= 1;
      mapAreaSquareMeters = (mapAreaSquareMeters - 45).clamp(0, 5000);
      notifyListeners();
      return;
    }
    if (zoneCount > 0) {
      _record();
      zoneCount -= 1;
      mapAreaSquareMeters = (mapAreaSquareMeters - 120).clamp(0, 5000);
      if (zoneCount == 0 && robotMode != RobotMode.charging) {
        robotMode = RobotMode.parking;
      }
      notifyListeners();
      return;
    }
    if (hasMap && perimeterPointCount > 12) {
      _record();
      perimeterPointCount -= 4;
      mapAreaSquareMeters = (mapAreaSquareMeters - 160).clamp(210, 5000);
      notifyListeners();
    }
  }

  void undo() {
    if (_history.isEmpty) {
      return;
    }
    final snapshot = _history.removeLast();
    hasMap = snapshot.hasMap;
    zoneCount = snapshot.zoneCount;
    noGoCount = snapshot.noGoCount;
    channelCount = snapshot.channelCount;
    perimeterPointCount = snapshot.perimeterPointCount;
    mapAreaSquareMeters = snapshot.mapAreaSquareMeters;
    robotMode = snapshot.robotMode;
    notifyListeners();
  }

  void handlePrimaryDashboardAction() {
    if (!hasMap) {
      createMap();
      return;
    }
  }

  void startMowing({
    required bool reorderZones,
    required bool clearProgressBeforeStart,
  }) {
    final missionId = backendMissions.isEmpty ? null : backendMissions.first.id;
    robotService.startMowing(missionId: hasZones ? missionId : null);
    if (!hasMap) {
      createMap();
      return;
    }
    _record();
    robotMode = RobotMode.mowing;
    if (!hasZones) {
      missionName = 'Gesamte Karte';
      missionZoneLabel = 'Gesamte Karte';
    }
    nextMissionLabel = hasZones ? nextMissionLabel : 'Manueller Lauf';
    notifyListeners();
  }

  void pauseMowing() {
    robotService.emergencyStop();
    if (robotMode != RobotMode.mowing) {
      return;
    }
    _record();
    robotMode = RobotMode.parking;
    notifyListeners();
  }

  void sendToStation() {
    robotService.startDocking();
    if (!hasMap) {
      return;
    }
    _record();
    robotMode = RobotMode.charging;
    notifyListeners();
  }

  void sendManualDriveCommand({
    required double linear,
    required double angular,
  }) {
    robotService.sendDriveCommand(linear, angular);
  }

  Future<String?> setManualMowMotor(bool on) async {
    try {
      await robotService.setMowMotor(on);
      return null;
    } catch (error) {
      return 'Mähmotor konnte nicht geschaltet werden: $error';
    }
  }

  void emergencyStopRobot() {
    robotService.emergencyStop();
  }

  Future<void> ensureAppUpdateInfoLoaded({bool force = false}) async {
    if (isLoadingAppUpdateInfo) {
      return;
    }
    if (!force && hasLoadedAppUpdateInfo) {
      return;
    }

    isLoadingAppUpdateInfo = true;
    appUpdateInfoError = null;
    notifyListeners();

    try {
      final results = await Future.wait<Object?>(<Future<Object?>>[
        otaRepository.currentInstalledVersion(),
        otaRepository.fetchLatestAppRelease(),
      ]);
      installedAppVersion = results[0] as String?;
      latestAppRelease = results[1] as AppRelease?;
    } catch (error) {
      appUpdateInfoError = 'Release-Check fehlgeschlagen: $error';
    } finally {
      isLoadingAppUpdateInfo = false;
      notifyListeners();
    }
  }

  Future<String?> downloadAndInstallLatestAppUpdate() async {
    final release = latestAppRelease;
    if (release == null) {
      return 'Kein GitHub Release geladen.';
    }
    if (!release.hasApkAsset) {
      return 'Im Release wurde kein APK-Asset gefunden.';
    }

    appOtaInstallState = const OtaInstallState(
      isDownloading: true,
      progress: 0,
      statusMessage: 'Lade APK herunter...',
    );
    notifyListeners();

    try {
      final filePath = await otaRepository.downloadApk(
        release: release,
        onProgress: (progress) {
          appOtaInstallState = OtaInstallState(
            isDownloading: true,
            progress: progress,
            statusMessage: 'Lade APK herunter...',
          );
          notifyListeners();
        },
      );

      appOtaInstallState = OtaInstallState(
        isDownloading: false,
        progress: 1,
        downloadedFilePath: filePath,
        statusMessage: 'APK heruntergeladen. Installation wird geöffnet.',
      );
      notifyListeners();

      return installDownloadedAppUpdate(filePath: filePath);
    } catch (error) {
      appOtaInstallState = OtaInstallState(
        isDownloading: false,
        errorMessage: 'APK-Download fehlgeschlagen: $error',
      );
      notifyListeners();
      return appOtaInstallState.errorMessage;
    }
  }

  Future<String?> installDownloadedAppUpdate({String? filePath}) async {
    final targetPath = filePath ?? appOtaInstallState.downloadedFilePath;
    if (targetPath == null || targetPath.trim().isEmpty) {
      return 'Keine heruntergeladene APK gefunden.';
    }

    try {
      final result = await otaRepository.openDownloadedApk(targetPath);
      if (result != null) {
        appOtaInstallState = appOtaInstallState.copyWith(
          errorMessage: result,
          clearStatusMessage: true,
        );
      } else {
        appOtaInstallState = appOtaInstallState.copyWith(
          statusMessage: 'Installer geöffnet.',
          clearErrorMessage: true,
        );
      }
      notifyListeners();
      return result;
    } catch (error) {
      final message = 'Installation konnte nicht gestartet werden: $error';
      appOtaInstallState = appOtaInstallState.copyWith(
        errorMessage: message,
        clearStatusMessage: true,
      );
      notifyListeners();
      return message;
    }
  }

  void handleSecondaryDashboardAction() {
    if (!hasMap || !hasZones) {
      return;
    }
    _record();
    if (robotMode == RobotMode.mowing || robotMode == RobotMode.delayed) {
      robotMode = RobotMode.parking;
    } else {
      robotMode = RobotMode.charging;
    }
    notifyListeners();
  }

  void toggleOnlyWhenDry() {
    _record();
    onlyWhenDry = !onlyWhenDry;
    notifyListeners();
  }

  void toggleRequiresHighBattery() {
    _record();
    requiresHighBattery = !requiresHighBattery;
    notifyListeners();
  }

  void cycleSchedule() {
    _record();
    nextMissionLabel = switch (nextMissionLabel) {
      'Heute 17:00' => 'Mo / Mi / Fr',
      'Mo / Mi / Fr' => 'Nur manuell',
      _ => 'Heute 17:00',
    };
    notifyListeners();
  }

  List<String> get availableMissionZones {
    return mapGeometry.zones
        .map((zone) => zone.name.trim())
        .where((name) => name.isNotEmpty)
        .toList(growable: false);
  }

  AppMission? missionById(String missionId) {
    for (final mission in _missions) {
      if (mission.id == missionId) {
        return mission;
      }
    }
    return null;
  }

  AppMission createMission() {
    _missionCounter += 1;
    final mission = AppMission(
      id: 'mission-$_missionCounter',
      name: 'Mission $_missionCounter',
      zones: const <String>[],
      scheduleLabel: 'Nur manuell',
      statusLabel: 'Entwurf',
      isEnabled: false,
      onlyWhenDry: true,
      requiresHighBattery: true,
      optimizeOrder: true,
    );
    _missions = <AppMission>[..._missions, mission];
    notifyListeners();
    return mission;
  }

  void updateMission(AppMission updatedMission) {
    _missions = _missions
        .map(
          (mission) =>
              mission.id == updatedMission.id ? updatedMission : mission,
        )
        .toList();
    notifyListeners();
  }

  void deleteMission(String missionId) {
    _missions = _missions
        .where((mission) => mission.id != missionId)
        .toList(growable: false);
    if (_missions.isEmpty) {
      missionName = 'Keine Mission';
      missionZoneLabel = hasZones ? 'Zonen verfügbar' : 'Gesamte Karte';
      nextMissionLabel = 'Nur manuell';
    } else {
      final first = _missions.first;
      missionName = first.name;
      missionZoneLabel = first.zones.isEmpty
          ? 'Gesamte Karte'
          : first.zones.join(', ');
      nextMissionLabel = first.scheduleLabel;
    }
    notifyListeners();
  }

  void startMission(String missionId) {
    final mission = missionById(missionId);
    if (mission == null) {
      return;
    }
    _record();
    robotMode = RobotMode.mowing;
    missionName = mission.name;
    missionZoneLabel = mission.zones.isEmpty
        ? 'Gesamte Karte'
        : mission.zones.join(', ');
    nextMissionLabel = mission.scheduleLabel;
    _missions = _missions
        .map(
          (item) => item.id == missionId
              ? item.copyWith(statusLabel: 'Aktiv', isEnabled: true)
              : item.copyWith(
                  statusLabel: item.isEnabled ? 'Geplant' : item.statusLabel,
                ),
        )
        .toList();
    notifyListeners();
  }

  void startSingleZoneMowing(String zoneName) {
    if (zoneName.trim().isEmpty) {
      return;
    }
    _record();
    robotMode = RobotMode.mowing;
    missionName = 'Zonenstart';
    missionZoneLabel = zoneName;
    nextMissionLabel = 'Nur manuell';
    _missions = _missions
        .map(
          (item) => item.copyWith(
            statusLabel: item.isEnabled ? 'Geplant' : item.statusLabel,
          ),
        )
        .toList();
    notifyListeners();
  }

  void clearNotifications() {
    if (unreadNotifications == 0) {
      return;
    }
    _record();
    unreadNotifications = 0;
    _lastNotifiedMessage = connectionStatus.uiMessage;
    _lastNotifiedError = connectionStatus.lastError;
    notifyListeners();
  }

  void setRobotMode(RobotMode mode) {
    _record();
    robotMode = mode;
    notifyListeners();
  }

  void _record() {
    _history.add(
      _AppSnapshot(
        hasMap: hasMap,
        zoneCount: zoneCount,
        noGoCount: noGoCount,
        channelCount: channelCount,
        perimeterPointCount: perimeterPointCount,
        mapAreaSquareMeters: mapAreaSquareMeters,
        robotMode: robotMode,
      ),
    );
  }

  Future<RobotStatus> _connectAndRemember(SavedRobot robot) async {
    final status = await robotService.connect(robot);
    if (status.connectionState == ConnectionStateKind.connected) {
      final persistedRobot = connectedRobot ?? robot;
      await robotStorage.saveConnectedRobot(persistedRobot);
      savedRobots = await robotStorage.loadSavedRobots();
      notifyListeners();
    }
    return status;
  }

  Future<void> _restoreSavedRobots() async {
    try {
      savedRobots = await robotStorage.loadSavedRobots();
      final savedMap = await appStateStorage.loadMapGeometry();
      if (savedMap != null) {
        applyMapGeometry(savedMap, notify: false);
        mapEditorState = mapEditorState.copyWith(
          setupStage: hasBaseMapReady
              ? MapSetupStage.save
              : MapSetupStage.perimeter,
        );
      }
      final activeRobot = await robotStorage.loadActiveRobot();
      if (activeRobot != null) {
        connectedRobot = activeRobot;
        hasKnownRobot = true;
        robotName = activeRobot.name;
      }
    } finally {
      isRestoringSession = false;
      notifyListeners();
    }
  }

  RobotMode _robotModeFromStatus(RobotStatus status) {
    final phase = status.statePhase;
    if (phase == 'mowing') {
      return RobotMode.mowing;
    }
    if (phase == 'waiting_for_rain') {
      return RobotMode.delayed;
    }
    if (status.chargerConnected == true ||
        phase == 'charging' ||
        phase == 'docked') {
      return RobotMode.charging;
    }
    return RobotMode.parking;
  }

  int _polygonAreaSquareMeters(List<MapPoint> points) {
    if (points.length < 3) {
      return 0;
    }
    const earthRadiusM = 6378137.0;
    var latSumRad = 0.0;
    for (final point in points) {
      latSumRad += point.y * math.pi / 180.0;
    }
    final lat0Rad = latSumRad / points.length;

    final projected = points
        .map(
          (point) => (
            x: earthRadiusM * point.x * math.pi / 180.0 * math.cos(lat0Rad),
            y: earthRadiusM * point.y * math.pi / 180.0,
          ),
        )
        .toList(growable: false);

    double area = 0;
    for (var index = 0; index < projected.length; index += 1) {
      final current = projected[index];
      final next = projected[(index + 1) % projected.length];
      area += current.x * next.y - next.x * current.y;
    }
    return (area.abs() / 2).round();
  }

  void _setMapGeometry(MapGeometry next, {bool notify = true}) {
    _mapUndoStack.add(mapGeometry);
    _mapRedoStack.clear();
    previewRoutePoints = const <MapPoint>[];
    previewError = null;
    isLoadingPreview = false;
    applyMapGeometry(next, notify: notify);
  }

  bool _sameGeometry(MapGeometry left, MapGeometry right) {
    bool samePoints(List<MapPoint> a, List<MapPoint> b) {
      if (a.length != b.length) {
        return false;
      }
      for (var index = 0; index < a.length; index += 1) {
        if (a[index].x != b[index].x || a[index].y != b[index].y) {
          return false;
        }
      }
      return true;
    }

    bool samePolygonList(List<List<MapPoint>> a, List<List<MapPoint>> b) {
      if (a.length != b.length) {
        return false;
      }
      for (var index = 0; index < a.length; index += 1) {
        if (!samePoints(a[index], b[index])) {
          return false;
        }
      }
      return true;
    }

    if (!samePoints(left.perimeter, right.perimeter) ||
        !samePoints(left.dock, right.dock) ||
        !samePolygonList(left.noGoZones, right.noGoZones) ||
        left.zones.length != right.zones.length) {
      return false;
    }

    for (var index = 0; index < left.zones.length; index += 1) {
      final leftZone = left.zones[index];
      final rightZone = right.zones[index];
      if (leftZone.id != rightZone.id ||
          leftZone.name != rightZone.name ||
          leftZone.priority != rightZone.priority ||
          leftZone.mowingDirection != rightZone.mowingDirection ||
          leftZone.mowingProfile != rightZone.mowingProfile ||
          !samePoints(leftZone.points, rightZone.points)) {
        return false;
      }
    }

    return true;
  }
}

class _RobotStateAdapter implements RobotStateSink {
  AppController? controller;

  @override
  void applyConnectedRobot(SavedRobot? robot, {bool notify = true}) {
    controller?.applyConnectedRobot(robot, notify: notify);
  }

  @override
  void applyConnectionStatus(RobotStatus status, {bool notify = true}) {
    controller?.applyConnectionStatus(status, notify: notify);
  }

  @override
  void applyMapGeometry(MapGeometry geometry, {bool notify = true}) {
    controller?.applyMapGeometry(geometry, notify: notify);
  }

  @override
  void applyMissions(List<Mission> missions, {bool notify = true}) {
    controller?.applyMissions(missions, notify: notify);
  }

  @override
  void applyStoredMaps(
    String activeId,
    List<StoredMap> maps, {
    bool notify = true,
  }) {
    controller?.applyStoredMaps(activeId, maps, notify: notify);
  }

  @override
  void applyTelemetry(RobotStatus status) {
    controller?.applyTelemetry(status);
  }
}

class AppMission {
  const AppMission({
    required this.id,
    required this.name,
    required this.zones,
    required this.scheduleLabel,
    required this.statusLabel,
    required this.isEnabled,
    required this.onlyWhenDry,
    required this.requiresHighBattery,
    required this.optimizeOrder,
  });

  final String id;
  final String name;
  final List<String> zones;
  final String scheduleLabel;
  final String statusLabel;
  final bool isEnabled;
  final bool onlyWhenDry;
  final bool requiresHighBattery;
  final bool optimizeOrder;

  AppMission copyWith({
    String? id,
    String? name,
    List<String>? zones,
    String? scheduleLabel,
    String? statusLabel,
    bool? isEnabled,
    bool? onlyWhenDry,
    bool? requiresHighBattery,
    bool? optimizeOrder,
  }) {
    return AppMission(
      id: id ?? this.id,
      name: name ?? this.name,
      zones: zones ?? this.zones,
      scheduleLabel: scheduleLabel ?? this.scheduleLabel,
      statusLabel: statusLabel ?? this.statusLabel,
      isEnabled: isEnabled ?? this.isEnabled,
      onlyWhenDry: onlyWhenDry ?? this.onlyWhenDry,
      requiresHighBattery: requiresHighBattery ?? this.requiresHighBattery,
      optimizeOrder: optimizeOrder ?? this.optimizeOrder,
    );
  }
}

class _AppSnapshot {
  const _AppSnapshot({
    required this.hasMap,
    required this.zoneCount,
    required this.noGoCount,
    required this.channelCount,
    required this.perimeterPointCount,
    required this.mapAreaSquareMeters,
    required this.robotMode,
  });

  final bool hasMap;
  final int zoneCount;
  final int noGoCount;
  final int channelCount;
  final int perimeterPointCount;
  final int mapAreaSquareMeters;
  final RobotMode robotMode;
}
