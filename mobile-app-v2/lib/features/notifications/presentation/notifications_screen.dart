import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';

class NotificationsScreen extends StatelessWidget {
  const NotificationsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final status = controller.connectionStatus;
    final latestMessage = status.uiMessage;
    final latestError = status.lastError;

    return Scaffold(
      backgroundColor: const Color(0xFFF5F7F2),
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
          _SectionCard(
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
                const SizedBox(height: 12),
                Text(
                  '${controller.unreadNotifications} ungelesene Hinweise',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                const SizedBox(height: 8),
                TextButton(
                  onPressed: controller.unreadNotifications > 0
                      ? controller.clearNotifications
                      : null,
                  child: const Text('Alle als gelesen markieren'),
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          const _SectionCard(
            title: 'Hinweis',
            icon: Icons.info_outline_rounded,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  'Statuswechsel, Warnungen und Bedienhinweise laufen bewusst in einen sekundären Bereich.',
                ),
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

class _SectionCard extends StatelessWidget {
  const _SectionCard({
    required this.title,
    required this.icon,
    required this.child,
  });

  final String title;
  final IconData icon;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                Icon(icon, color: const Color(0xFF2F7D4A)),
                const SizedBox(width: 10),
                Text(
                  title,
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            child,
          ],
        ),
      ),
    );
  }
}
