import 'package:go_router/go_router.dart';

import '../features/dashboard/presentation/dashboard_screen.dart';
import '../features/discovery/presentation/discovery_screen.dart';
import '../features/map_editor/presentation/map_editor_screen.dart';
import '../features/missions/presentation/missions_screen.dart';
import '../features/service/presentation/service_screen.dart';
import '../features/statistics/presentation/statistics_screen.dart';
import '../shared/widgets/app_shell.dart';

final GoRouter appRouter = GoRouter(
  initialLocation: '/discover',
  routes: <RouteBase>[
    GoRoute(
      path: '/discover',
      builder: (context, state) => const DiscoveryScreen(),
    ),
    ShellRoute(
      builder: (context, state, child) => AppShell(child: child),
      routes: <RouteBase>[
        GoRoute(
          path: '/dashboard',
          builder: (context, state) => const DashboardScreen(),
        ),
        GoRoute(
          path: '/map',
          builder: (context, state) => const MapEditorScreen(),
        ),
        GoRoute(
          path: '/missions',
          builder: (context, state) => const MissionsScreen(),
        ),
        GoRoute(
          path: '/service',
          builder: (context, state) => const ServiceScreen(),
        ),
        GoRoute(
          path: '/statistics',
          builder: (context, state) => const StatisticsScreen(),
        ),
      ],
    ),
  ],
);
