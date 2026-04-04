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
    this.piVersion,
    this.lastError,
    // Diagnose fields
    this.batteryVoltage,
    this.chargerConnected,
    this.mcuConnected,
    this.mcuVersion,
    this.runtimeHealth,
    this.uptimeSeconds,
    this.mowFaultActive,
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
  final String? piVersion;
  final String? lastError;

  // Diagnose
  final double? batteryVoltage;
  final bool? chargerConnected;
  final bool? mcuConnected;
  final double? mcuVersion;
  final bool? runtimeHealth;
  final int? uptimeSeconds;
  final bool? mowFaultActive;

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
    String? piVersion,
    String? lastError,
    double? batteryVoltage,
    bool? chargerConnected,
    bool? mcuConnected,
    double? mcuVersion,
    bool? runtimeHealth,
    int? uptimeSeconds,
    bool? mowFaultActive,
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
      piVersion: piVersion ?? this.piVersion,
      lastError: lastError ?? this.lastError,
      batteryVoltage: batteryVoltage ?? this.batteryVoltage,
      chargerConnected: chargerConnected ?? this.chargerConnected,
      mcuConnected: mcuConnected ?? this.mcuConnected,
      mcuVersion: mcuVersion ?? this.mcuVersion,
      runtimeHealth: runtimeHealth ?? this.runtimeHealth,
      uptimeSeconds: uptimeSeconds ?? this.uptimeSeconds,
      mowFaultActive: mowFaultActive ?? this.mowFaultActive,
    );
  }
}
