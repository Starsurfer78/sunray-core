class SavedRobot {
  const SavedRobot({
    required this.id,
    required this.name,
    required this.lastHost,
    required this.port,
    required this.lastSeen,
  });

  final String id;
  final String name;
  final String lastHost;
  final int port;
  final DateTime lastSeen;
}