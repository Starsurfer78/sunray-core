enum ConnectionStateKind {
  disconnected,
  discovering,
  connecting,
  connected,
  error,
}

class RobotStatus {
  const RobotStatus({
    required this.connectionState,
    this.robotName,
    this.batteryPercent,
    this.rtkState,
    this.statePhase,
    this.x,
    this.y,
    this.heading,
    this.gpsLat,
    this.gpsLon,
    this.gpsNumSv,
    this.gpsDgpsAgeMs,
    this.piVersion,
    this.lastError,
    this.batteryVoltage,
    this.chargerConnected,
    this.mcuConnected,
    this.mcuVersion,
    this.runtimeHealth,
    this.uptimeSeconds,
    this.mowFaultActive,
    this.uiMessage,
    this.uiSeverity,
    this.mowDistanceM,
    this.mowDurationSec,
  });

  final ConnectionStateKind connectionState;
  final String? robotName;
  final int? batteryPercent;
  final String? rtkState;
  final String? statePhase;
  final double? x;
  final double? y;
  final double? heading;
  final double? gpsLat;
  final double? gpsLon;
  final int? gpsNumSv;
  final int? gpsDgpsAgeMs;
  final String? piVersion;
  final String? lastError;
  final double? batteryVoltage;
  final bool? chargerConnected;
  final bool? mcuConnected;
  final String? mcuVersion;
  final String? runtimeHealth;
  final int? uptimeSeconds;
  final bool? mowFaultActive;
  final String? uiMessage;
  final String? uiSeverity;
  final double? mowDistanceM;
  final int? mowDurationSec;

  RobotStatus copyWith({
    ConnectionStateKind? connectionState,
    String? robotName,
    int? batteryPercent,
    String? rtkState,
    String? statePhase,
    double? x,
    double? y,
    double? heading,
    double? gpsLat,
    double? gpsLon,
    int? gpsNumSv,
    int? gpsDgpsAgeMs,
    String? piVersion,
    String? lastError,
    double? batteryVoltage,
    bool? chargerConnected,
    bool? mcuConnected,
    String? mcuVersion,
    String? runtimeHealth,
    int? uptimeSeconds,
    bool? mowFaultActive,
    String? uiMessage,
    String? uiSeverity,
    double? mowDistanceM,
    int? mowDurationSec,
  }) {
    return RobotStatus(
      connectionState: connectionState ?? this.connectionState,
      robotName: robotName ?? this.robotName,
      batteryPercent: batteryPercent ?? this.batteryPercent,
      rtkState: rtkState ?? this.rtkState,
      statePhase: statePhase ?? this.statePhase,
      x: x ?? this.x,
      y: y ?? this.y,
      heading: heading ?? this.heading,
      gpsLat: gpsLat ?? this.gpsLat,
      gpsLon: gpsLon ?? this.gpsLon,
      gpsNumSv: gpsNumSv ?? this.gpsNumSv,
      gpsDgpsAgeMs: gpsDgpsAgeMs ?? this.gpsDgpsAgeMs,
      piVersion: piVersion ?? this.piVersion,
      lastError: lastError ?? this.lastError,
      batteryVoltage: batteryVoltage ?? this.batteryVoltage,
      chargerConnected: chargerConnected ?? this.chargerConnected,
      mcuConnected: mcuConnected ?? this.mcuConnected,
      mcuVersion: mcuVersion ?? this.mcuVersion,
      runtimeHealth: runtimeHealth ?? this.runtimeHealth,
      uptimeSeconds: uptimeSeconds ?? this.uptimeSeconds,
      mowFaultActive: mowFaultActive ?? this.mowFaultActive,
      uiMessage: uiMessage ?? this.uiMessage,
      uiSeverity: uiSeverity ?? this.uiSeverity,
      mowDistanceM: mowDistanceM ?? this.mowDistanceM,
      mowDurationSec: mowDurationSec ?? this.mowDurationSec,
    );
  }
}
