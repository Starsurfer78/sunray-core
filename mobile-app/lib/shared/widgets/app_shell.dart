import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class AppShell extends StatelessWidget {
  const AppShell({
    required this.child,
    super.key,
  });

  final Widget child;

  static const List<_NavItem> _items = <_NavItem>[
    _NavItem(label: 'Dashboard', icon: Icons.space_dashboard_rounded, path: '/dashboard'),
    _NavItem(label: 'Karte', icon: Icons.map_rounded, path: '/map'),
    _NavItem(label: 'Missionen', icon: Icons.route_rounded, path: '/missions'),
    _NavItem(label: 'Service', icon: Icons.miscellaneous_services_rounded, path: '/service'),
  ];

  @override
  Widget build(BuildContext context) {
    final String location = GoRouterState.of(context).uri.toString();
    final int index = _items.indexWhere((item) => location.startsWith(item.path));
    final int currentIndex = index < 0 ? 0 : index;

    return Scaffold(
      body: child,
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: currentIndex,
        onTap: (value) => context.go(_items[value].path),
        items: _items
            .map(
              (item) => BottomNavigationBarItem(
                icon: Icon(item.icon),
                label: item.label,
              ),
            )
            .toList(),
      ),
    );
  }
}

class _NavItem {
  const _NavItem({
    required this.label,
    required this.icon,
    required this.path,
  });

  final String label;
  final IconData icon;
  final String path;
}

