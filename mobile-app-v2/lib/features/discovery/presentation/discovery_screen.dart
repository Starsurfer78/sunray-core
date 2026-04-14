import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../core/config/app_constants.dart';
import '../../../data/discovery/discovery_repository.dart';
import '../../../data/discovery/discovery_service.dart';
import '../../../domain/robot/discovered_robot.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../domain/robot/saved_robot.dart';

class DiscoveryScreen extends StatefulWidget {
  const DiscoveryScreen({super.key});

  @override
  State<DiscoveryScreen> createState() => _DiscoveryScreenState();
}

class _DiscoveryScreenState extends State<DiscoveryScreen> {
  late final DiscoveryRepository _repository;
  late Stream<List<DiscoveredRobot>> _robotsStream;

  @override
  void initState() {
    super.initState();
    _repository = DiscoveryRepository(DiscoveryService());
    _robotsStream = _repository.watchRobots();
  }

  void _refreshDiscovery() {
    setState(() {
      _robotsStream = _repository.watchRobots();
    });
  }

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);

    return Scaffold(
      backgroundColor: const Color(0xFFE8E8EC),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(20, 24, 20, 20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                'Mähroboter verbinden',
                style: Theme.of(
                  context,
                ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.w800),
              ),
              const SizedBox(height: 8),
              Text(
                'Discovery wird nur für die Ersteinrichtung oder Wiederverbindung verwendet. Sobald Alfred bekannt ist, startet die App direkt im Dashboard.',
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: const Color(0xFF6C6C70),
                ),
              ),
              const SizedBox(height: 24),
              Expanded(
                child: _DiscoveryCard(
                  savedRobots: controller.savedRobots,
                  robotsStream: _robotsStream,
                  onRefresh: _refreshDiscovery,
                  onConnectSaved: (robot) => _connectSavedRobot(
                    controller,
                    robot,
                  ),
                  onForgetSaved: (robot) => controller.forgetSavedRobot(robot.id),
                  onConnectDiscovered: (robot) => _connectDiscoveredRobot(
                    controller,
                    robot,
                  ),
                  onConnectFallback: () => _connectFallbackRobot(controller),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _connectDiscoveredRobot(
    AppController controller,
    DiscoveredRobot robot,
  ) async {
    final status = await controller.connectRobot(
      robot: SavedRobot(
        id: robot.id,
        name: robot.name,
        lastHost: robot.host,
        port: robot.port,
        lastSeen: DateTime.now(),
      ),
    );
    _handleConnectResult(status);
  }

  Future<void> _connectSavedRobot(
    AppController controller,
    SavedRobot robot,
  ) async {
    final status = await controller.connectRobot(robot: robot);
    _handleConnectResult(status);
  }

  Future<void> _connectFallbackRobot(AppController controller) async {
    final status = await controller.connectRobot(
      robot: SavedRobot(
        id: '${AppConstants.defaultRobotHost}:${AppConstants.defaultRobotPort}',
        name: controller.robotName,
        lastHost: AppConstants.defaultRobotHost,
        port: AppConstants.defaultRobotPort,
        lastSeen: DateTime.now(),
      ),
    );
    _handleConnectResult(status);
  }

  void _handleConnectResult(RobotStatus status) {
    if (!mounted) {
      return;
    }
    if (status.connectionState == ConnectionStateKind.connected) {
      context.go('/dashboard');
      return;
    }
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(status.lastError ?? 'Verbindung fehlgeschlagen.'),
      ),
    );
  }
}

class _DiscoveryCard extends StatelessWidget {
  const _DiscoveryCard({
    required this.savedRobots,
    required this.robotsStream,
    required this.onRefresh,
    required this.onConnectSaved,
    required this.onForgetSaved,
    required this.onConnectDiscovered,
    required this.onConnectFallback,
  });

  final List<SavedRobot> savedRobots;
  final Stream<List<DiscoveredRobot>> robotsStream;
  final VoidCallback onRefresh;
  final ValueChanged<SavedRobot> onConnectSaved;
  final ValueChanged<SavedRobot> onForgetSaved;
  final ValueChanged<DiscoveredRobot> onConnectDiscovered;
  final VoidCallback onConnectFallback;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              Expanded(
                child: Text(
                  'Discovery',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ),
              IconButton(
                tooltip: 'Neu suchen',
                onPressed: onRefresh,
                icon: const Icon(Icons.refresh_rounded),
              ),
            ],
          ),
          const SizedBox(height: 14),
          if (savedRobots.isNotEmpty) ...<Widget>[
            Text(
              'Zuletzt verbunden',
              style: Theme.of(context).textTheme.titleSmall,
            ),
            const SizedBox(height: 10),
            _SavedRobotTile(
              robot: savedRobots.first,
              isPrimary: true,
              onConnect: () => onConnectSaved(savedRobots.first),
              onForget: () => onForgetSaved(savedRobots.first),
            ),
            if (savedRobots.length > 1) ...<Widget>[
              const SizedBox(height: 16),
              Text(
                'Gespeicherte Hosts',
                style: Theme.of(context).textTheme.titleSmall,
              ),
              const SizedBox(height: 10),
              SizedBox(
                height: 144,
                child: ListView.separated(
                  itemCount: savedRobots.length - 1,
                  separatorBuilder: (context, index) => const SizedBox(height: 10),
                  itemBuilder: (context, index) {
                    final robot = savedRobots[index + 1];
                    return _SavedRobotTile(
                      robot: robot,
                      onConnect: () => onConnectSaved(robot),
                      onForget: () => onForgetSaved(robot),
                    );
                  },
                ),
              ),
            ],
            const SizedBox(height: 18),
            Text(
              'Per mDNS gefunden',
              style: Theme.of(context).textTheme.titleSmall,
            ),
            const SizedBox(height: 10),
          ],
          Expanded(
            child: StreamBuilder<List<DiscoveredRobot>>(
              stream: robotsStream,
              builder: (context, snapshot) {
                final robots = snapshot.data ?? const <DiscoveredRobot>[];
                if (snapshot.connectionState == ConnectionState.waiting && robots.isEmpty) {
                  return const Center(
                    child: CircularProgressIndicator(strokeWidth: 2.4),
                  );
                }
                if (robots.isEmpty) {
                  return Center(
                    child: Text(
                      'Noch kein Roboter per mDNS gefunden. Du kannst erneut suchen oder den Standard-Host verwenden.',
                      style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                        color: const Color(0xFF6C6C70),
                      ),
                      textAlign: TextAlign.center,
                    ),
                  );
                }
                return ListView.separated(
                  itemCount: robots.length,
                  separatorBuilder: (context, index) => const SizedBox(height: 12),
                  itemBuilder: (context, index) {
                    final robot = robots[index];
                    return _RobotTile(
                      robot: robot,
                      onConnect: () => onConnectDiscovered(robot),
                    );
                  },
                );
              },
            ),
          ),
          const SizedBox(height: 16),
          SizedBox(
            width: double.infinity,
            child: OutlinedButton(
              onPressed: onConnectFallback,
              child: const Text('Mit Standard-Host verbinden'),
            ),
          ),
        ],
      ),
    );
  }
}

class _RobotTile extends StatelessWidget {
  const _RobotTile({
    required this.robot,
    required this.onConnect,
  });

  final DiscoveredRobot robot;
  final VoidCallback onConnect;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFFF7F9F4),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Row(
        children: <Widget>[
          Container(
            width: 44,
            height: 44,
            decoration: BoxDecoration(
              color: const Color(0x142F7D4A),
              borderRadius: BorderRadius.circular(14),
            ),
            child: const Icon(
              Icons.grass_rounded,
              color: Color(0xFF2F7D4A),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  robot.name,
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                const SizedBox(height: 2),
                Text(
                  '${robot.host}:${robot.port}${robot.statusHint == null ? '' : ' • ${robot.statusHint}'}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ),
          FilledButton(
            onPressed: onConnect,
            child: const Text('Verbinden'),
          ),
        ],
      ),
    );
  }
}

class _SavedRobotTile extends StatelessWidget {
  const _SavedRobotTile({
    required this.robot,
    required this.onConnect,
    required this.onForget,
    this.isPrimary = false,
  });

  final SavedRobot robot;
  final VoidCallback onConnect;
  final VoidCallback onForget;
  final bool isPrimary;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: isPrimary ? const Color(0xFFF0F7EE) : const Color(0xFFF7F9F4),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Row(
        children: <Widget>[
          Container(
            width: 44,
            height: 44,
            decoration: BoxDecoration(
              color: const Color(0x142F7D4A),
              borderRadius: BorderRadius.circular(14),
            ),
            child: const Icon(
              Icons.history_rounded,
              color: Color(0xFF2F7D4A),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  robot.name,
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                const SizedBox(height: 2),
                Text(
                  '${robot.lastHost}:${robot.port}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ),
          IconButton(
            tooltip: 'Entfernen',
            onPressed: onForget,
            icon: const Icon(Icons.close_rounded),
          ),
          FilledButton(
            onPressed: onConnect,
            child: const Text('Verbinden'),
          ),
        ],
      ),
    );
  }
}
