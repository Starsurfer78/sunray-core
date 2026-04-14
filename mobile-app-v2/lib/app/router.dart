import 'package:flutter/widgets.dart';
import 'package:go_router/go_router.dart';

import '../features/dashboard/presentation/dashboard_screen.dart';
import '../features/controller/presentation/controller_screen.dart';
import '../features/discovery/presentation/discovery_screen.dart';
import '../features/discovery/presentation/startup_gate_screen.dart';
import '../features/map/presentation/add_map_object_screen.dart';
import '../features/map/presentation/map_screen.dart';
import '../features/mowing/presentation/mow_settings_screen.dart';
import '../features/missions/presentation/mission_detail_screen.dart';
import '../features/missions/presentation/missions_screen.dart';
import '../features/notifications/presentation/notifications_screen.dart';
import '../features/service/presentation/service_screen.dart';
import '../features/settings/presentation/settings_screen.dart';
import '../features/statistics/presentation/statistics_screen.dart';
import 'app_controller.dart';

GoRouter buildAppRouter(AppController controller) {
  return GoRouter(
    initialLocation: '/',
    refreshListenable: controller,
    redirect: (BuildContext context, GoRouterState state) {
      final location = state.matchedLocation;
      final isStartup = location == '/';
      final isDiscovery = location == '/discover';

      if (controller.isRestoringSession) {
        return isStartup ? null : '/';
      }

      if (!controller.hasKnownRobot && !isDiscovery) {
        return '/discover';
      }

      if (controller.hasKnownRobot && isDiscovery) {
        return '/dashboard';
      }

      if (isStartup) {
        return controller.hasKnownRobot ? '/dashboard' : '/discover';
      }

      return null;
    },
    routes: <RouteBase>[
      GoRoute(
        path: '/',
        builder: (BuildContext context, GoRouterState state) =>
            const StartupGateScreen(),
      ),
      GoRoute(
        path: '/discover',
        builder: (BuildContext context, GoRouterState state) =>
            const DiscoveryScreen(),
      ),
      GoRoute(
        path: '/dashboard',
        builder: (BuildContext context, GoRouterState state) =>
            const DashboardScreen(),
        routes: <RouteBase>[
          GoRoute(
            path: 'controller',
            builder: (BuildContext context, GoRouterState state) =>
                const ControllerScreen(),
          ),
          GoRoute(
            path: 'mow',
            builder: (BuildContext context, GoRouterState state) =>
                const MowSettingsScreen(),
          ),
        ],
      ),
      GoRoute(
        path: '/map',
        builder: (BuildContext context, GoRouterState state) =>
            const MapScreen(),
        routes: <RouteBase>[
          GoRoute(
            path: 'add-object',
            builder: (BuildContext context, GoRouterState state) =>
                const AddMapObjectScreen(),
          ),
        ],
      ),
      GoRoute(
        path: '/missions',
        builder: (BuildContext context, GoRouterState state) =>
            const MissionsScreen(),
        routes: <RouteBase>[
          GoRoute(
            path: 'detail/:missionId',
            builder: (BuildContext context, GoRouterState state) =>
                MissionDetailScreen(
                  missionId: state.pathParameters['missionId']!,
                ),
          ),
        ],
      ),
      GoRoute(
        path: '/service',
        builder: (BuildContext context, GoRouterState state) =>
            const ServiceScreen(),
      ),
      GoRoute(
        path: '/notifications',
        builder: (BuildContext context, GoRouterState state) =>
            const NotificationsScreen(),
      ),
      GoRoute(
        path: '/settings',
        builder: (BuildContext context, GoRouterState state) =>
            const SettingsScreen(),
      ),
      GoRoute(
        path: '/statistics',
        builder: (BuildContext context, GoRouterState state) =>
            const StatisticsScreen(),
      ),
    ],
  );
}
