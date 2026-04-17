import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../shared/widgets/robot_map_view.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  static const double _cardHeight = 208;
  static const double _statusPillBottom = _cardHeight + 12;
  static const double _missionTeaserBottom = _cardHeight + 64;
  static const Color _alfredGreen = Color(0xFF2F7D4A);

  String? _selectedZoneId;

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final selectedZoneName = _selectedZoneName(controller);
    final view = _DashboardViewData.fromController(
      controller,
      selectedZoneName: selectedZoneName,
    );

    return Scaffold(
      body: ColoredBox(
        color: const Color(0xFFE8E8EC),
        child: SafeArea(
          child: Stack(
            children: <Widget>[
              Positioned.fill(
                child: view.hasMap
                    ? RobotMapView(
                        map: controller.mapGeometry,
                        status: controller.connectionStatus,
                        interactive: false,
                        selectedZoneIds: _selectedZoneId == null
                            ? const <String>[]
                            : <String>[_selectedZoneId!],
                        highlightActiveZoneId: null,
                        showCenterButton: false,
                        onZoneTap: (zoneId) {
                          setState(() {
                            _selectedZoneId = _selectedZoneId == zoneId
                                ? null
                                : zoneId;
                          });
                        },
                      )
                    : const ColoredBox(color: Color(0xFFE8E8EC)),
              ),
              Positioned(
                top: 8,
                left: 12,
                right: 12,
                child: _DashboardTopBar(
                  view: view,
                  robotName: controller.robotName,
                  unreadNotifications: controller.unreadNotifications,
                  onOpenDashboard: () => context.go('/dashboard'),
                  onOpenHelp: () => _openHelp(context),
                  onOpenNotifications: () => context.push('/notifications'),
                ),
              ),
              if (view.hasMap)
                Positioned(
                  right: 12,
                  top: 118,
                  child: _DashboardQuickNav(
                    onOpenController: () =>
                        context.push('/dashboard/controller'),
                    onOpenMap: () => context.push('/map'),
                    onOpenMissions: () => context.push('/missions'),
                    onOpenService: () => context.push('/service'),
                  ),
                ),
              Positioned(
                left: 12,
                bottom: _statusPillBottom,
                child: _StatusPill(view: view),
              ),
              if (view.hasMap) ...<Widget>[
                if (view.showMissionTeaser)
                  Positioned(
                    left: 12,
                    right: 12,
                    bottom: _missionTeaserBottom,
                    child: _MissionTeaser(
                      title: controller.missionName,
                      subtitle: view.missionSubtitle!,
                      onTap: () => context.push('/missions'),
                    ),
                  ),
              ],
              Positioned(
                left: 0,
                right: 0,
                bottom: 0,
                child: _DashboardActionModule(
                  view: view,
                  mapAreaSquareMeters: controller.mapAreaSquareMeters,
                  batteryPercent: controller.batteryPercent,
                  gpsLabel:
                      controller.connectionStatus.rtkState ?? 'GPS unbekannt',
                  isConnected:
                      controller.connectionStatus.connectionState ==
                      ConnectionStateKind.connected,
                  isDocked:
                      controller.connectionStatus.chargerConnected == true,
                  onPrimaryPressed: () {
                    if (!controller.hasMap) {
                      controller.beginMapCreation();
                      context.push('/map');
                      return;
                    }
                    if (view.primaryAction == _DashboardPrimaryAction.pause) {
                      controller.pauseMowing();
                      return;
                    }
                    context.push('/dashboard/mow');
                  },
                  onSecondaryPressed: switch (view.secondaryAction) {
                    _DashboardSecondaryAction.none => null,
                    _DashboardSecondaryAction.zoneMow =>
                      selectedZoneName == null
                          ? null
                          : () => _openZoneMowSheet(
                              context,
                              controller,
                              selectedZoneName,
                            ),
                    _DashboardSecondaryAction.mission => () => context.push(
                      '/missions',
                    ),
                    _DashboardSecondaryAction.station =>
                      controller.sendToStation,
                  },
                  onAntennaFinder: () => _openAntennaFinder(context),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  String? _selectedZoneName(AppController controller) {
    final zoneId = _selectedZoneId;
    if (zoneId == null) {
      return null;
    }
    for (final zone in controller.mapGeometry.zones) {
      if (zone.id == zoneId) {
        return zone.name;
      }
    }
    return null;
  }

  Future<void> _openZoneMowSheet(
    BuildContext context,
    AppController controller,
    String zoneName,
  ) {
    return showModalBottomSheet<void>(
      context: context,
      builder: (sheetContext) {
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  'Zone mähen',
                  style: Theme.of(sheetContext).textTheme.titleMedium,
                ),
                const SizedBox(height: 12),
                Text(
                  'Alfred startet diese Zone direkt aus dem Dashboard.',
                  style: Theme.of(sheetContext).textTheme.bodyMedium,
                ),
                const SizedBox(height: 14),
                Row(
                  children: <Widget>[
                    const Expanded(child: Text('Zone')),
                    Text(
                      zoneName,
                      style: Theme.of(sheetContext).textTheme.bodyMedium
                          ?.copyWith(fontWeight: FontWeight.w800),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                Row(
                  children: <Widget>[
                    const Expanded(child: Text('Startart')),
                    Text(
                      'Nur manuell',
                      style: Theme.of(sheetContext).textTheme.bodyMedium
                          ?.copyWith(fontWeight: FontWeight.w800),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                FilledButton(
                  onPressed: () {
                    controller.startSingleZoneMowing(zoneName);
                    Navigator.of(sheetContext).pop();
                  },
                  style: FilledButton.styleFrom(
                    backgroundColor: const Color(0xFF2F7D4A),
                    foregroundColor: Colors.white,
                    minimumSize: const Size.fromHeight(52),
                  ),
                  child: Text('$zoneName jetzt mähen'),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _openHelp(BuildContext context) {
    return showDialog<void>(
      context: context,
      builder: (dialogContext) {
        return AlertDialog(
          title: const Text('Hilfe & Feedback'),
          content: const Text(
            'Nutze Karte für die Einrichtung, Missionen für Planung und Service für Diagnose. Das Dashboard bleibt der Alltags-Einstieg.',
          ),
          actions: <Widget>[
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('Schließen'),
            ),
          ],
        );
      },
    );
  }

  Future<void> _openAntennaFinder(BuildContext context) {
    return showDialog<void>(
      context: context,
      builder: (dialogContext) {
        return AlertDialog(
          title: const Text('Standortfinder für die Antenne'),
          content: const Text(
            'Platziere die Antenne dort, wo Alfred freie Sicht zum Himmel und Abstand zu Wänden, Metallkanten und dichtem Baumbestand hat.',
          ),
          actions: <Widget>[
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('Schließen'),
            ),
          ],
        );
      },
    );
  }
}

class _DashboardViewData {
  const _DashboardViewData({
    required this.hasMap,
    required this.hasZones,
    required this.stage,
    required this.topStatus,
    required this.statusLabel,
    required this.primaryLabel,
    required this.primaryAction,
    required this.selectedZoneName,
    required this.secondaryLabel,
    required this.secondaryAction,
    required this.nextActionValue,
    required this.nextActionLabel,
    required this.missionSubtitle,
  });

  factory _DashboardViewData.fromController(
    AppController controller, {
    String? selectedZoneName,
  }) {
    return _DashboardViewData(
      hasMap: controller.hasMap,
      hasZones: controller.hasZones,
      stage: controller.dashboardStage,
      topStatus: controller.topStatus,
      statusLabel: controller.statusLabel,
      primaryLabel: controller.primaryDashboardLabel,
      primaryAction: switch (controller.dashboardStage) {
        DashboardStage.mapZonesMowing => _DashboardPrimaryAction.pause,
        _ => _DashboardPrimaryAction.start,
      },
      selectedZoneName: selectedZoneName,
      secondaryLabel:
          selectedZoneName != null &&
              controller.dashboardStage != DashboardStage.mapZonesMowing
          ? 'Zone mähen'
          : controller.secondaryDashboardLabel,
      secondaryAction:
          selectedZoneName != null &&
              controller.dashboardStage != DashboardStage.mapZonesMowing
          ? _DashboardSecondaryAction.zoneMow
          : switch (controller.dashboardStage) {
              DashboardStage.mapZonesCharging =>
                _DashboardSecondaryAction.mission,
              DashboardStage.mapZonesDelayed =>
                _DashboardSecondaryAction.mission,
              DashboardStage.mapZonesParking =>
                _DashboardSecondaryAction.station,
              DashboardStage.mapZonesMowing =>
                _DashboardSecondaryAction.station,
              _ => _DashboardSecondaryAction.none,
            },
      nextActionValue: controller.nextActionValue,
      nextActionLabel: controller.nextActionLabel,
      missionSubtitle: controller.missionSubtitle,
    );
  }

  final bool hasMap;
  final bool hasZones;
  final DashboardStage stage;
  final String topStatus;
  final String statusLabel;
  final String primaryLabel;
  final _DashboardPrimaryAction primaryAction;
  final String? selectedZoneName;
  final String? secondaryLabel;
  final _DashboardSecondaryAction secondaryAction;
  final String nextActionValue;
  final String nextActionLabel;
  final String? missionSubtitle;

  bool get showMissionTeaser =>
      stage != DashboardStage.mapNoZones && missionSubtitle != null;

  IconData get statusIcon {
    return switch (statusLabel) {
      'Lädt' => Icons.home_rounded,
      'Geparkt' => Icons.home_rounded,
      'Mäht' => Icons.grass_rounded,
      'Mähen verzögert' => Icons.cloud_queue_rounded,
      _ => Icons.info_outline_rounded,
    };
  }
}

enum _DashboardPrimaryAction { start, pause }

enum _DashboardSecondaryAction { none, mission, station, zoneMow }

class _DashboardTopBar extends StatelessWidget {
  const _DashboardTopBar({
    required this.view,
    required this.robotName,
    required this.unreadNotifications,
    required this.onOpenDashboard,
    required this.onOpenHelp,
    required this.onOpenNotifications,
  });

  final _DashboardViewData view;
  final String robotName;
  final int unreadNotifications;
  final VoidCallback onOpenDashboard;
  final VoidCallback onOpenHelp;
  final VoidCallback onOpenNotifications;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: <Widget>[
        _DarkCircleAction(icon: Icons.home_rounded, onTap: onOpenDashboard),
        const SizedBox(width: 10),
        Expanded(
          child: _OverlayPill(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 13),
            child: Row(
              children: <Widget>[
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisSize: MainAxisSize.min,
                    children: <Widget>[
                      Text(
                        robotName,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context).textTheme.titleMedium
                            ?.copyWith(
                              color: const Color(0xFF1C1C1E),
                              fontWeight: FontWeight.w800,
                            ),
                      ),
                      Text(
                        view.topStatus,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: const Color(0xFF6C6C70),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                const Icon(
                  Icons.keyboard_arrow_down_rounded,
                  color: Color(0xFF3A3A3C),
                ),
              ],
            ),
          ),
        ),
        const SizedBox(width: 10),
        _WhiteCircleAction(icon: Icons.headset_mic_outlined, onTap: onOpenHelp),
        const SizedBox(width: 8),
        _NotificationAction(
          unreadNotifications: unreadNotifications,
          onTap: onOpenNotifications,
        ),
      ],
    );
  }
}

class _StatusPill extends StatelessWidget {
  const _StatusPill({required this.view});

  final _DashboardViewData view;

  @override
  Widget build(BuildContext context) {
    return _OverlayPill(
      borderRadius: 16,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Icon(view.statusIcon, size: 16, color: const Color(0xFF3A3A3C)),
          const SizedBox(width: 8),
          Text(
            view.statusLabel,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
              color: const Color(0xFF1C1C1E),
              fontWeight: FontWeight.w800,
            ),
          ),
        ],
      ),
    );
  }
}

class _MissionTeaser extends StatelessWidget {
  const _MissionTeaser({
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(18),
      child: _OverlayPill(
        borderRadius: 18,
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        child: Row(
          children: <Widget>[
            Container(
              width: 40,
              height: 40,
              decoration: BoxDecoration(
                color: const Color(0x142F7D4A),
                borderRadius: BorderRadius.circular(14),
              ),
              child: const Icon(
                Icons.route_rounded,
                color: _DashboardScreenState._alfredGreen,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    title,
                    style: Theme.of(context).textTheme.titleSmall?.copyWith(
                      color: const Color(0xFF1C1C1E),
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    subtitle,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFF6C6C70),
                    ),
                  ),
                ],
              ),
            ),
            const Icon(Icons.chevron_right_rounded, color: Color(0xFF8A8A90)),
          ],
        ),
      ),
    );
  }
}

class _DashboardActionModule extends StatelessWidget {
  const _DashboardActionModule({
    required this.view,
    required this.mapAreaSquareMeters,
    required this.batteryPercent,
    required this.gpsLabel,
    required this.isConnected,
    required this.isDocked,
    required this.onPrimaryPressed,
    required this.onSecondaryPressed,
    required this.onAntennaFinder,
  });

  final _DashboardViewData view;
  final int mapAreaSquareMeters;
  final int batteryPercent;
  final String gpsLabel;
  final bool isConnected;
  final bool isDocked;
  final VoidCallback onPrimaryPressed;
  final VoidCallback? onSecondaryPressed;
  final VoidCallback onAntennaFinder;

  @override
  Widget build(BuildContext context) {
    final bottomPadding = MediaQuery.paddingOf(context).bottom;

    return Container(
      padding: EdgeInsets.fromLTRB(
        16,
        view.hasMap ? 14 : 18,
        16,
        bottomPadding > 0 ? bottomPadding : 16,
      ),
      decoration: const BoxDecoration(
        color: Color(0xFFF2F2F6),
        borderRadius: BorderRadius.only(
          topLeft: Radius.circular(22),
          topRight: Radius.circular(22),
        ),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: Color(0x14000000),
            blurRadius: 20,
            offset: Offset(0, -4),
          ),
        ],
      ),
      child: view.hasMap
          ? _MappedActionContent(
              nextActionValue: view.nextActionValue,
              nextActionLabel: view.nextActionLabel,
              mapAreaSquareMeters: mapAreaSquareMeters,
              batteryPercent: batteryPercent,
              gpsLabel: gpsLabel,
              isConnected: isConnected,
              isDocked: isDocked,
              primaryLabel: view.primaryLabel,
              secondaryLabel: view.secondaryLabel,
              onPrimaryPressed: onPrimaryPressed,
              onSecondaryPressed: onSecondaryPressed,
            )
          : _NoMapActionContent(
              onPrimaryPressed: onPrimaryPressed,
              onAntennaFinder: onAntennaFinder,
            ),
    );
  }
}

class _MappedActionContent extends StatelessWidget {
  const _MappedActionContent({
    required this.nextActionValue,
    required this.nextActionLabel,
    required this.mapAreaSquareMeters,
    required this.batteryPercent,
    required this.gpsLabel,
    required this.isConnected,
    required this.isDocked,
    required this.primaryLabel,
    required this.secondaryLabel,
    required this.onPrimaryPressed,
    required this.onSecondaryPressed,
  });

  final String nextActionValue;
  final String nextActionLabel;
  final int mapAreaSquareMeters;
  final int batteryPercent;
  final String gpsLabel;
  final bool isConnected;
  final bool isDocked;
  final String primaryLabel;
  final String? secondaryLabel;
  final VoidCallback onPrimaryPressed;
  final VoidCallback? onSecondaryPressed;

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Row(
          children: <Widget>[
            Icon(
              isConnected
                  ? Icons.wifi_tethering_rounded
                  : Icons.wifi_tethering_error_rounded,
              size: 16,
              color: isConnected
                  ? const Color(0xFF2F7D4A)
                  : const Color(0xFFB91C1C),
            ),
            const SizedBox(width: 10),
            Text(
              gpsLabel,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: const Color(0xFF1C1C1E),
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(width: 10),
            Icon(
              isDocked ? Icons.home_rounded : Icons.route_rounded,
              size: 18,
              color: const Color(0xFF6C6C70),
            ),
            const SizedBox(width: 6),
            Text(
              isDocked ? 'Dock' : '$batteryPercent%',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: const Color(0xFF1C1C1E),
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 16),
        Row(
          children: <Widget>[
            Expanded(
              child: _StatBlock(
                value: '$mapAreaSquareMeters m²',
                label: 'Kartenfläche',
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _StatBlock(value: nextActionValue, label: nextActionLabel),
            ),
          ],
        ),
        const SizedBox(height: 18),
        if (secondaryLabel == null)
          SizedBox(
            width: double.infinity,
            child: _PrimaryActionButton(
              label: primaryLabel,
              onPressed: onPrimaryPressed,
            ),
          )
        else
          Row(
            children: <Widget>[
              Expanded(
                child: _PrimaryActionButton(
                  label: primaryLabel,
                  onPressed: onPrimaryPressed,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _SecondaryActionButton(
                  label: secondaryLabel!,
                  onPressed: onSecondaryPressed ?? () {},
                ),
              ),
            ],
          ),
      ],
    );
  }
}

class _NoMapActionContent extends StatelessWidget {
  const _NoMapActionContent({
    required this.onPrimaryPressed,
    required this.onAntennaFinder,
  });

  final VoidCallback onPrimaryPressed;
  final VoidCallback onAntennaFinder;

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        Container(
          width: 88,
          height: 88,
          decoration: BoxDecoration(
            color: Colors.white,
            borderRadius: BorderRadius.circular(28),
            border: Border.all(color: const Color(0xFFD8D8DD)),
          ),
          child: const Icon(
            Icons.map_outlined,
            size: 42,
            color: Color(0xFFFF6B2B),
          ),
        ),
        const SizedBox(height: 18),
        Text(
          'Es gibt noch keine Karte.',
          textAlign: TextAlign.center,
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
            color: const Color(0xFF1C1C1E),
            fontWeight: FontWeight.w800,
          ),
        ),
        const SizedBox(height: 8),
        Text(
          'Lege die erste Karte an, damit Alfred hier Missionen, Routenkontext und Alltagsaktionen anzeigen kann.',
          textAlign: TextAlign.center,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            color: const Color(0xFF6C6C70),
            fontSize: 14,
          ),
        ),
        const SizedBox(height: 14),
        TextButton(
          onPressed: onAntennaFinder,
          child: const Text('Standortfinder für die Antenne'),
        ),
        const SizedBox(height: 14),
        SizedBox(
          width: double.infinity,
          child: _PrimaryActionButton(
            label: '+ Karte anlegen',
            onPressed: onPrimaryPressed,
          ),
        ),
      ],
    );
  }
}

class _DashboardQuickNav extends StatelessWidget {
  const _DashboardQuickNav({
    required this.onOpenController,
    required this.onOpenMap,
    required this.onOpenMissions,
    required this.onOpenService,
  });

  final VoidCallback onOpenController;
  final VoidCallback onOpenMap;
  final VoidCallback onOpenMissions;
  final VoidCallback onOpenService;

  @override
  Widget build(BuildContext context) {
    return _OverlayPill(
      borderRadius: 18,
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
      child: Column(
        children: <Widget>[
          _QuickNavButton(
            icon: Icons.sports_esports_rounded,
            label: 'Controller',
            onTap: onOpenController,
          ),
          const SizedBox(height: 8),
          _QuickNavButton(
            icon: Icons.map_outlined,
            label: 'Karte',
            onTap: onOpenMap,
          ),
          const SizedBox(height: 8),
          _QuickNavButton(
            icon: Icons.event_note_rounded,
            label: 'Missionen',
            onTap: onOpenMissions,
          ),
          const SizedBox(height: 8),
          _QuickNavButton(
            icon: Icons.miscellaneous_services_outlined,
            label: 'Service',
            onTap: onOpenService,
          ),
        ],
      ),
    );
  }
}

class _QuickNavButton extends StatelessWidget {
  const _QuickNavButton({
    required this.icon,
    required this.label,
    required this.onTap,
  });

  final IconData icon;
  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: Colors.white,
      borderRadius: BorderRadius.circular(14),
      child: InkWell(
        borderRadius: BorderRadius.circular(14),
        onTap: onTap,
        child: SizedBox(
          width: 84,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
            child: Column(
              children: <Widget>[
                Icon(icon, size: 20, color: _DashboardScreenState._alfredGreen),
                const SizedBox(height: 6),
                Text(
                  label,
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: const Color(0xFF1F2A22),
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _StatBlock extends StatelessWidget {
  const _StatBlock({required this.value, required this.label});

  final String value;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(
          value,
          style: Theme.of(context).textTheme.titleLarge?.copyWith(
            color: const Color(0xFF1C1C1E),
            fontSize: 28,
            fontWeight: FontWeight.w800,
          ),
        ),
        const SizedBox(height: 2),
        Text(
          label,
          style: Theme.of(
            context,
          ).textTheme.bodySmall?.copyWith(color: const Color(0xFF6C6C70)),
        ),
      ],
    );
  }
}

class _OverlayPill extends StatelessWidget {
  const _OverlayPill({
    required this.child,
    required this.padding,
    this.borderRadius = 18,
  });

  final Widget child;
  final EdgeInsets padding;
  final double borderRadius;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF2FFFFFF),
        borderRadius: BorderRadius.circular(borderRadius),
        border: Border.all(color: const Color(0x22FFFFFF)),
        boxShadow: const <BoxShadow>[
          BoxShadow(
            color: Color(0x12000000),
            blurRadius: 12,
            offset: Offset(0, 4),
          ),
        ],
      ),
      child: Padding(padding: padding, child: child),
    );
  }
}

class _DarkCircleAction extends StatelessWidget {
  const _DarkCircleAction({required this.icon, required this.onTap});

  final IconData icon;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xFF3A3A3C),
      shape: const CircleBorder(),
      child: InkWell(
        customBorder: const CircleBorder(),
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Icon(icon, color: Colors.white, size: 20),
        ),
      ),
    );
  }
}

class _WhiteCircleAction extends StatelessWidget {
  const _WhiteCircleAction({required this.icon, required this.onTap});

  final IconData icon;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xF2FFFFFF),
      shape: const CircleBorder(),
      child: InkWell(
        customBorder: const CircleBorder(),
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Icon(icon, color: const Color(0xFF1C1C1E), size: 20),
        ),
      ),
    );
  }
}

class _NotificationAction extends StatelessWidget {
  const _NotificationAction({
    required this.unreadNotifications,
    required this.onTap,
  });

  final int unreadNotifications;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Stack(
      clipBehavior: Clip.none,
      children: <Widget>[
        _WhiteCircleAction(
          icon: Icons.chat_bubble_outline_rounded,
          onTap: onTap,
        ),
        if (unreadNotifications > 0)
          Positioned(
            top: -2,
            right: -1,
            child: Container(
              height: 18,
              constraints: const BoxConstraints(minWidth: 18),
              padding: const EdgeInsets.symmetric(horizontal: 4),
              decoration: const BoxDecoration(
                color: _DashboardScreenState._alfredGreen,
                shape: BoxShape.circle,
              ),
              alignment: Alignment.center,
              child: Text(
                unreadNotifications > 9 ? '9+' : '$unreadNotifications',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Colors.white,
                  fontSize: 10,
                  fontWeight: FontWeight.w800,
                ),
              ),
            ),
          ),
      ],
    );
  }
}

class _PrimaryActionButton extends StatelessWidget {
  const _PrimaryActionButton({required this.label, required this.onPressed});

  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    return FilledButton(
      onPressed: onPressed,
      style: FilledButton.styleFrom(
        backgroundColor: _DashboardScreenState._alfredGreen,
        foregroundColor: Colors.white,
        minimumSize: const Size.fromHeight(54),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.labelLarge?.copyWith(
          color: Colors.white,
          fontWeight: FontWeight.w800,
        ),
      ),
    );
  }
}

class _SecondaryActionButton extends StatelessWidget {
  const _SecondaryActionButton({required this.label, required this.onPressed});

  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    return OutlinedButton(
      onPressed: onPressed,
      style: OutlinedButton.styleFrom(
        foregroundColor: const Color(0xFF3A3A3C),
        side: const BorderSide(color: Color(0xFFD0D0D6)),
        minimumSize: const Size.fromHeight(54),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.labelLarge?.copyWith(
          color: const Color(0xFF3A3A3C),
          fontWeight: FontWeight.w800,
        ),
      ),
    );
  }
}
