import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/section_card.dart';

class NotificationsScreen extends ConsumerWidget {
  const NotificationsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final status = ref.watch(connectionStateProvider);
    final latestMessage = status.uiMessage;
    final latestError = status.lastError;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Benachrichtigungen'),
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
            title: 'Aktuell',
            icon: Icons.notifications_active_outlined,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  latestMessage?.isNotEmpty == true
                      ? latestMessage!
                      : 'Aktuell liegt keine neue Meldung vor.',
                ),
                if (latestError != null && latestError.isNotEmpty) ...<Widget>[
                  const SizedBox(height: 8),
                  Text(
                    latestError,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: Colors.orange.shade300,
                        ),
                  ),
                ],
              ],
            ),
          ),
          const SizedBox(height: 12),
          const SectionCard(
            title: 'Hinweis',
            icon: Icons.info_outline_rounded,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text('Statuswechsel, Warnungen und Bedienhinweise laufen bewusst in einen sekundären Bereich.'),
                SizedBox(height: 8),
                Text('Damit bleibt das Dashboard kartenzentriert und ruhig.'),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
