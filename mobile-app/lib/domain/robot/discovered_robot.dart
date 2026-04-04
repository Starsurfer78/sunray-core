class DiscoveredRobot {
  const DiscoveredRobot({
    required this.id,
    required this.name,
    required this.host,
    required this.port,
    this.statusHint,
  });

  final String id;
  final String name;
  final String host;
  final int port;
  final String? statusHint;
}

