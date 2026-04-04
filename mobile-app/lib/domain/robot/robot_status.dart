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
    this.lastError,
  });

  final ConnectionStateKind connectionState;
  final String? robotName;
  final int? batteryPercent;
  final String? rtkState;
  final String? statePhase;
  final double? x;
  final double? y;
  final double? heading;
  final String? lastError;

  RobotStatus copyWith({
    ConnectionStateKind? connectionState,
    String? robotName,
    int? batteryPercent,
    String? rtkState,
    String? statePhase,
    double? x,
    double? y,
    double? heading,
    String? lastError,
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
      lastError: lastError ?? this.lastError,
    );
  }
}
