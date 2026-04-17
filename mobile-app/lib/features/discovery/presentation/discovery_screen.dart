import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../core/config/app_constants.dart';
import '../../../domain/robot/discovered_robot.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../domain/robot/saved_robot.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/section_card.dart';

class DiscoveryScreen extends ConsumerStatefulWidget {
  const DiscoveryScreen({super.key});

  @override
  ConsumerState<DiscoveryScreen> createState() => _DiscoveryScreenState();
}

class _DiscoveryScreenState extends ConsumerState<DiscoveryScreen> {
  @override
  Widget build(BuildContext context) {
    final robotsAsync = ref.watch(discoveredRobotsStreamProvider);
    final rememberedRobotAsync = ref.watch(rememberedRobotProvider);
    final connectionState = ref.watch(connectionStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Roboter finden'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Neu suchen',
            onPressed: _refreshDiscovery,
            icon: const Icon(Icons.refresh_rounded),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: <Widget>[
          if (connectionState.connectionState == ConnectionStateKind.error &&
              connectionState.lastError != null)
            Padding(
              padding: const EdgeInsets.only(bottom: 16),
              child: SectionCard(
                title: 'Verbindungsfehler',
                child: Text(
                  connectionState.lastError!,
                  style: theme.textTheme.bodySmall,
                ),
              ),
            ),
          if (activeRobot != null)
            Padding(
              padding: const EdgeInsets.only(bottom: 16),
              child: SectionCard(
                title: 'Aktiver Roboter',
                child: _ActiveRobotCard(
                  robot: activeRobot,
                  connectionState: connectionState,
                  onDisconnect: _disconnectCurrentRobot,
                  onSwitchRobot: _switchRobot,
                ),
              ),
            ),
          rememberedRobotAsync.when(
            data: (robot) => robotsAsync.when(
              data: (robots) => _buildRememberedRobotSection(
                context: context,
                connectionState: connectionState,
                rememberedRobot: robot,
                robots: robots,
              ),
              loading: () => _buildRememberedRobotSection(
                context: context,
                connectionState: connectionState,
                rememberedRobot: robot,
                robots: const <DiscoveredRobot>[],
              ),
              error: (_, __) => _buildRememberedRobotSection(
                context: context,
                connectionState: connectionState,
                rememberedRobot: robot,
                robots: const <DiscoveredRobot>[],
              ),
            ),
            loading: () => const SizedBox.shrink(),
            error: (_, __) => const SizedBox.shrink(),
          ),
          SectionCard(
            title: 'Suche nach Robotern...',
            trailing: TextButton.icon(
              onPressed: _refreshDiscovery,
              icon: const Icon(Icons.refresh_rounded, size: 18),
              label: const Text('Neu suchen'),
            ),
            child: robotsAsync.when(
              data: (robots) => _DiscoveryResults(
                robots: robots,
                connectionState: connectionState,
                onConnect: (robot) => _connectDiscoveredRobot(context, robot),
              ),
              error: (error, _) => Text(
                'Robotersuche fehlgeschlagen: $error',
                style: theme.textTheme.bodySmall,
              ),
              loading: () => Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    'Die App sucht per mDNS nach Alfred im gleichen WLAN.',
                    style: theme.textTheme.bodySmall,
                  ),
                  const SizedBox(height: 16),
                  const LinearProgressIndicator(),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),
          const _ManualConnectSection(),
        ],
      ),
    );
  }

  Widget _buildRememberedRobotSection({
    required BuildContext context,
    required RobotStatus connectionState,
    required SavedRobot? rememberedRobot,
    required List<DiscoveredRobot> robots,
  }) {
    if (rememberedRobot == null) return const SizedBox.shrink();

    final updatedDiscovery = robots.cast<DiscoveredRobot?>().firstWhere(
          (entry) =>
              entry != null &&
              entry.name == rememberedRobot.name &&
              entry.host != rememberedRobot.lastHost,
          orElse: () => null,
        );

    return Padding(
      padding: const EdgeInsets.only(bottom: 16),
      child: SectionCard(
        title: 'Zuletzt verbunden',
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            _RememberedRobotCard(
              robot: rememberedRobot,
              connectionState: connectionState,
              onConnect: () => _connectSavedRobot(context, rememberedRobot),
              onForget: () => _forgetRememberedRobot(rememberedRobot),
            ),
            if (updatedDiscovery != null) ...<Widget>[
              const SizedBox(height: 12),
              DecoratedBox(
                decoration: BoxDecoration(
                  color: Theme.of(context)
                      .colorScheme
                      .surfaceContainerHighest
                      .withValues(alpha: 0.24),
                  borderRadius: BorderRadius.circular(14),
                  border: Border.all(color: Theme.of(context).dividerColor),
                ),
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      const Text('Neue IP erkannt'),
                      const SizedBox(height: 6),
                      Text(
                        '${updatedDiscovery.name} wurde unter ${updatedDiscovery.host}:${updatedDiscovery.port} gefunden.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 10),
                      Align(
                        alignment: Alignment.centerLeft,
                        child: FilledButton.tonal(
                          onPressed: () => _connectDiscoveredRobot(
                              context, updatedDiscovery),
                          child: const Text('Neue IP verwenden'),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Future<void> _connectDiscoveredRobot(
    BuildContext context,
    DiscoveredRobot robot,
  ) async {
    final savedRobot = SavedRobot(
      id: robot.id,
      name: robot.name,
      lastHost: robot.host,
      port: robot.port,
      lastSeen: DateTime.now(),
    );
    await _connect(context, savedRobot);
  }

  Future<void> _connectSavedRobot(
    BuildContext context,
    SavedRobot robot,
  ) async {
    final refreshedRobot = SavedRobot(
      id: robot.id,
      name: robot.name,
      lastHost: robot.lastHost,
      port: robot.port,
      lastSeen: DateTime.now(),
    );
    await _connect(context, refreshedRobot);
  }

  Future<void> _connect(
    BuildContext context,
    SavedRobot robot,
  ) async {
    final status =
        await ref.read(robotConnectionControllerProvider).connect(robot);
    if (context.mounted &&
        status.connectionState == ConnectionStateKind.connected) {
      context.go('/dashboard');
    }
  }

  Future<void> _disconnectCurrentRobot() async {
    await ref.read(robotConnectionControllerProvider).disconnect();
  }

  Future<void> _switchRobot() async {
    await ref.read(robotConnectionControllerProvider).switchRobot();
  }

  Future<void> _forgetRememberedRobot(SavedRobot robot) async {
    final activeRobot = ref.read(activeRobotProvider);
    if (activeRobot?.id == robot.id) {
      await ref.read(robotConnectionControllerProvider).disconnect();
    }
    await ref.read(robotStorageProvider).forgetRobot(robot.id);
    ref.invalidate(savedRobotsProvider);
    ref.invalidate(rememberedRobotProvider);
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('${robot.name} wurde entfernt.')),
      );
    }
  }

  void _refreshDiscovery() {
    ref.invalidate(discoveredRobotsStreamProvider);
  }
}

class _ActiveRobotCard extends StatelessWidget {
  const _ActiveRobotCard({
    required this.robot,
    required this.connectionState,
    required this.onDisconnect,
    required this.onSwitchRobot,
  });

  final SavedRobot robot;
  final RobotStatus connectionState;
  final Future<void> Function() onDisconnect;
  final Future<void> Function() onSwitchRobot;

  @override
  Widget build(BuildContext context) {
    final connected =
        connectionState.connectionState == ConnectionStateKind.connected;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(robot.name, style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 4),
        Text(
          '${robot.lastHost}:${robot.port} · ${connected ? 'verbunden' : 'offline'}',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 12),
        Wrap(
          spacing: 12,
          runSpacing: 12,
          children: <Widget>[
            OutlinedButton(
              onPressed: onDisconnect,
              child: const Text('Trennen'),
            ),
            FilledButton.tonal(
              onPressed: onSwitchRobot,
              child: const Text('Anderen wählen'),
            ),
          ],
        ),
      ],
    );
  }
}

class _RememberedRobotCard extends StatelessWidget {
  const _RememberedRobotCard({
    required this.robot,
    required this.connectionState,
    required this.onConnect,
    required this.onForget,
  });

  final SavedRobot robot;
  final RobotStatus connectionState;
  final VoidCallback onConnect;
  final VoidCallback onForget;

  @override
  Widget build(BuildContext context) {
    final isBusy =
        connectionState.connectionState == ConnectionStateKind.connecting &&
            connectionState.robotName == robot.name;

    return Row(
      children: <Widget>[
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(robot.name, style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 4),
              Text(
                '${robot.lastHost}:${robot.port} · ${_lastSeenLabel(robot.lastSeen)}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
        FilledButton(
          onPressed: isBusy ? null : onConnect,
          child: isBusy
              ? const SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              : const Text('Verbinden'),
        ),
        const SizedBox(width: 8),
        IconButton(
          tooltip: 'Entfernen',
          onPressed: onForget,
          icon: const Icon(Icons.delete_outline_rounded),
        ),
      ],
    );
  }

  String _lastSeenLabel(DateTime lastSeen) {
    final now = DateTime.now();
    final diff = now.difference(lastSeen);
    if (diff.inMinutes < 1) return 'gerade eben';
    if (diff.inHours < 1) return 'vor ${diff.inMinutes} min';
    if (diff.inDays < 1) return 'vor ${diff.inHours} h';
    return 'vor ${diff.inDays} Tagen';
  }
}

class _DiscoveryResults extends StatelessWidget {
  const _DiscoveryResults({
    required this.robots,
    required this.connectionState,
    required this.onConnect,
  });

  final List<DiscoveredRobot> robots;
  final RobotStatus connectionState;
  final ValueChanged<DiscoveredRobot> onConnect;

  @override
  Widget build(BuildContext context) {
    if (robots.isEmpty) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            'Die App sucht per mDNS nach Alfred im gleichen WLAN.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
          const SizedBox(height: 16),
          Text(
            'Noch kein Roboter gefunden. Stelle sicher, dass Alfred im selben WLAN ist oder nutze den manuellen Verbindungsweg.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(
            'Gefundene Roboter im lokalen Netzwerk.',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 16),
        for (final robot in robots)
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: _RobotTile(
              name: robot.name,
              subtitle:
                  '${robot.host}:${robot.port} · ${robot.statusHint ?? 'gefunden'}',
                ctaLabel: 'Verbinden',
              isBusy: connectionState.connectionState ==
                      ConnectionStateKind.connecting &&
                  connectionState.robotName == robot.name,
              onPressed: () => onConnect(robot),
            ),
          ),
      ],
    );
  }
}

class _RobotTile extends StatelessWidget {
  const _RobotTile({
    required this.name,
    required this.subtitle,
    required this.ctaLabel,
    required this.onPressed,
    this.isBusy = false,
  });

  final String name;
  final String subtitle;
  final String ctaLabel;
  final VoidCallback onPressed;
  final bool isBusy;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: theme.cardColor,
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: theme.dividerColor),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: <Widget>[
            const Icon(Icons.precision_manufacturing_rounded, size: 28),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(name, style: theme.textTheme.titleMedium),
                  const SizedBox(height: 4),
                  Text(subtitle, style: theme.textTheme.bodySmall),
                ],
              ),
            ),
            FilledButton(
              onPressed: isBusy ? null : onPressed,
              child: isBusy
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : Text(ctaLabel),
            ),
          ],
        ),
      ),
    );
  }
}

class _ManualConnectSection extends StatelessWidget {
  const _ManualConnectSection();

  @override
  Widget build(BuildContext context) {
    return const SectionCard(
      title: 'Manuell verbinden',
      child: _ManualConnectForm(),
    );
  }
}

class _ManualConnectForm extends ConsumerStatefulWidget {
  const _ManualConnectForm();

  @override
  ConsumerState<_ManualConnectForm> createState() => _ManualConnectFormState();
}

class _ManualConnectFormState extends ConsumerState<_ManualConnectForm> {
  late final TextEditingController _ipController;
  late final TextEditingController _portController;
  bool _connecting = false;

  @override
  void initState() {
    super.initState();
    _ipController = TextEditingController();
    _portController = TextEditingController(
      text: AppConstants.defaultRobotPort.toString(),
    );
  }

  @override
  void dispose() {
    _ipController.dispose();
    _portController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: <Widget>[
        TextField(
          controller: _ipController,
          decoration: const InputDecoration(
            labelText: 'IP-Adresse',
            hintText: '192.168.1.42',
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _portController,
          keyboardType: TextInputType.number,
          decoration: const InputDecoration(
            labelText: 'Port',
            hintText: '8765',
          ),
        ),
        const SizedBox(height: 16),
        SizedBox(
          width: double.infinity,
          child: OutlinedButton(
            onPressed: _connecting ? null : _handleManualConnect,
            child: _connecting
                ? const SizedBox(
                    width: 18,
                    height: 18,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Text('Verbinden'),
          ),
        ),
      ],
    );
  }

  Future<void> _handleManualConnect() async {
    final host = _ipController.text.trim();
    final port = int.tryParse(_portController.text.trim()) ??
        AppConstants.defaultRobotPort;

    if (host.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Bitte eine IP-Adresse eingeben.')),
      );
      return;
    }

    setState(() => _connecting = true);
    final router = GoRouter.of(context);
    final messenger = ScaffoldMessenger.of(context);
    try {
      final robot = SavedRobot(
        id: '$host:$port',
        name: 'Alfred',
        lastHost: host,
        port: port,
        lastSeen: DateTime.now(),
      );

      final status =
          await ref.read(robotConnectionControllerProvider).connect(robot);
      if (!mounted) return;
      if (status.connectionState == ConnectionStateKind.connected) {
        router.go('/dashboard');
      } else {
        messenger.showSnackBar(
          SnackBar(
            content: Text(
              status.lastError ?? 'Verbindung fehlgeschlagen.',
            ),
          ),
        );
      }
    } finally {
      if (mounted) setState(() => _connecting = false);
    }
  }
}
