import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../providers/app_providers.dart';
import '../../domain/robot/robot_status.dart';

class AppShell extends ConsumerWidget {
  const AppShell({required this.child, super.key});

  final Widget child;

  static const List<_NavItem> _items = <_NavItem>[
    _NavItem(label: 'Dashboard', icon: Icons.space_dashboard_rounded, path: '/dashboard'),
    _NavItem(label: 'Karte',     icon: Icons.map_rounded,             path: '/map'),
    _NavItem(label: 'Missionen', icon: Icons.route_rounded,           path: '/missions'),
    _NavItem(label: 'Statistik', icon: Icons.bar_chart_rounded,       path: '/statistics'),
    _NavItem(label: 'Service',   icon: Icons.miscellaneous_services_rounded, path: '/service'),
  ];

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final String location = GoRouterState.of(context).uri.toString();
    final int index = _items.indexWhere((item) => location.startsWith(item.path));
    final int currentIndex = index < 0 ? 0 : index;

    final status = ref.watch(connectionStateProvider);
    final hasFault = status.connectionState == ConnectionStateKind.error ||
        status.statePhase == 'fault';

    return Scaffold(
      body: child,
      bottomNavigationBar: NavigationBar(
        selectedIndex: currentIndex,
        onDestinationSelected: (value) => context.go(_items[value].path),
        destinations: <NavigationDestination>[
          const NavigationDestination(
            icon: Icon(Icons.space_dashboard_outlined),
            selectedIcon: Icon(Icons.space_dashboard_rounded),
            label: 'Dashboard',
          ),
          const NavigationDestination(
            icon: Icon(Icons.map_outlined),
            selectedIcon: Icon(Icons.map_rounded),
            label: 'Karte',
          ),
          const NavigationDestination(
            icon: Icon(Icons.route_outlined),
            selectedIcon: Icon(Icons.route_rounded),
            label: 'Missionen',
          ),
          const NavigationDestination(
            icon: Icon(Icons.bar_chart_outlined),
            selectedIcon: Icon(Icons.bar_chart_rounded),
            label: 'Statistik',
          ),
          NavigationDestination(
            icon: Badge(
              isLabelVisible: hasFault,
              child: const Icon(Icons.miscellaneous_services_outlined),
            ),
            selectedIcon: Badge(
              isLabelVisible: hasFault,
              child: const Icon(Icons.miscellaneous_services_rounded),
            ),
            label: 'Service',
          ),
        ],
      ),
    );
  }
}

class _NavItem {
  const _NavItem({required this.label, required this.icon, required this.path});
  final String label;
  final IconData icon;
  final String path;
}
