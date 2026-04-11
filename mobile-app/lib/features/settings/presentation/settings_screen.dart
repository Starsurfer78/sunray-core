import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/section_card.dart';

class SettingsScreen extends ConsumerWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final activeRobot = ref.watch(activeRobotProvider);
    final versionAsync = ref.watch(currentAppVersionProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Einstellungen'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Dashboard',
            onPressed: () => context.go('/dashboard'),
            icon: const Icon(Icons.space_dashboard_outlined),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: <Widget>[
          SectionCard(
            title: 'Aktiver Roboter',
            icon: Icons.smart_toy_outlined,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(activeRobot?.name ?? 'Kein Roboter aktiv'),
                const SizedBox(height: 6),
                Text(
                  activeRobot == null
                      ? 'Verbinde Alfred zuerst ueber Discovery.'
                      : '${activeRobot.lastHost}:${activeRobot.port}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          SectionCard(
            title: 'App',
            icon: Icons.tune_rounded,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                versionAsync.when(
                  data: (version) => Text('Version $version'),
                  loading: () => const Text('Version wird geladen...'),
                  error: (_, __) => const Text('Version nicht verfuegbar'),
                ),
                const SizedBox(height: 8),
                const Text('Updates und tiefe Systemfunktionen bleiben weiterhin im Service-Bereich gebuendelt.'),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
