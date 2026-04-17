import 'dart:async';

import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';
import '../../../core/config/app_constants.dart';

class ServiceScreen extends StatelessWidget {
  const ServiceScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);

    return Scaffold(
      backgroundColor: const Color(0xFFF5F7F2),
      body: SafeArea(
        child: Column(
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 8, 16, 0),
              child: Row(
                children: <Widget>[
                  IconButton(
                    onPressed: () {
                      if (context.canPop()) {
                        context.pop();
                      } else {
                        context.go('/dashboard');
                      }
                    },
                    icon: const Icon(Icons.arrow_back_rounded),
                  ),
                  const SizedBox(width: 4),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        Text(
                          'Service',
                          style: Theme.of(context).textTheme.titleLarge
                              ?.copyWith(fontWeight: FontWeight.w800),
                        ),
                        Text(
                          'Verbindung, Diagnose und Systempflege',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    onPressed: () => _openRobotStateSheet(context),
                    icon: const Icon(Icons.tune_rounded),
                  ),
                ],
              ),
            ),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.fromLTRB(16, 16, 16, 24),
                children: <Widget>[
                  _ServiceSection(
                    title: 'Verbindung',
                    items: <_ServiceEntry>[
                      _ServiceEntry(
                        icon: Icons.wifi_rounded,
                        title: 'WLAN & Hostname',
                        subtitle: controller.hasKnownRobot
                            ? 'Verbundener Roboter • letzter Host bekannt'
                            : 'Kein bekannter Host',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'WLAN & Hostname',
                          rows: <_InfoRow>[
                            _InfoRow(
                              label: 'Hostname',
                              value: controller.hasKnownRobot
                                  ? '${controller.robotName}.local'
                                  : 'Unbekannt',
                            ),
                            const _InfoRow(
                              label: 'IP-Adresse',
                              value: 'Unbekannt',
                            ),
                            _InfoRow(
                              label: 'Verbindung',
                              value: controller.hasKnownRobot
                                  ? 'Verbunden'
                                  : 'Discovery erforderlich',
                            ),
                          ],
                        ),
                      ),
                      _ServiceEntry(
                        icon: Icons.hub_rounded,
                        title: 'MQTT-Status',
                        subtitle: controller.hasKnownRobot
                            ? 'App-Session aktiv'
                            : 'Kein Roboter verbunden',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'MQTT-Status',
                          rows: <_InfoRow>[
                            const _InfoRow(
                              label: 'Client-ID',
                              value: 'sunray-mobile',
                            ),
                            _InfoRow(
                              label: 'Status',
                              value: controller.hasKnownRobot
                                  ? 'Aus App-Sicht verbunden'
                                  : 'Warte auf Discovery',
                            ),
                            const _InfoRow(label: 'Keepalive', value: '30 s'),
                          ],
                        ),
                      ),
                      _ServiceEntry(
                        icon: Icons.network_check_rounded,
                        title: 'Verbindung testen',
                        subtitle: 'Ping, Discovery und Basisdienste prüfen',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'Verbindung testen',
                          rows: const <_InfoRow>[
                            _InfoRow(label: 'Discovery', value: 'Antwortet'),
                            _InfoRow(label: 'HTTP-API', value: 'Erreichbar'),
                            _InfoRow(label: 'MQTT', value: 'Session aktiv'),
                          ],
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  _ServiceSection(
                    title: 'Roboter',
                    items: <_ServiceEntry>[
                      _ServiceEntry(
                        icon: Icons.smart_toy_rounded,
                        title: 'Roboterstatus',
                        subtitle:
                            '${controller.statusLabel} • ${controller.batteryPercent}% Akku',
                        onTap: () => _openRobotStateSheet(context),
                      ),
                      _ServiceEntry(
                        icon: Icons.memory_rounded,
                        title: 'Firmware & Hardware',
                        subtitle: 'Versionen nicht aus Telemetrie verdrahtet',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'Firmware & Hardware',
                          rows: const <_InfoRow>[
                            _InfoRow(label: 'Pi', value: 'Unbekannt'),
                            _InfoRow(label: 'STM', value: 'Unbekannt'),
                            _InfoRow(
                              label: 'Hardware map',
                              value: 'Lokal nicht bekannt',
                            ),
                          ],
                        ),
                      ),
                      _ServiceEntry(
                        icon: Icons.straighten_rounded,
                        title: 'Kalibrierung',
                        subtitle: 'Status aus lokalem App-Modell',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'Kalibrierung',
                          rows: const <_InfoRow>[
                            _InfoRow(label: 'Kompass', value: 'Unbekannt'),
                            _InfoRow(label: 'IMU', value: 'Unbekannt'),
                            _InfoRow(
                              label: 'Letzter Lauf',
                              value: 'Nicht angebunden',
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  _ServiceSection(
                    title: 'Diagnose',
                    items: <_ServiceEntry>[
                      _ServiceEntry(
                        icon: Icons.insights_rounded,
                        title: 'Telemetrie-Log',
                        subtitle: 'Position, Akku und Missionsstatus',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'Telemetrie-Log',
                          rows: <_InfoRow>[
                            _InfoRow(
                              label: 'Missionsname',
                              value: controller.missionName,
                            ),
                            _InfoRow(
                              label: 'Status',
                              value: controller.statusLabel,
                            ),
                            _InfoRow(
                              label: 'Kartenfläche',
                              value: '${controller.mapAreaSquareMeters} m²',
                            ),
                          ],
                        ),
                      ),
                      _ServiceEntry(
                        icon: Icons.bar_chart_rounded,
                        title: 'Statistik',
                        subtitle:
                            'Mähsessions und Gesamtzeit aus Verlauf laden',
                        onTap: () => context.push('/statistics'),
                      ),
                      _ServiceEntry(
                        icon: Icons.gps_fixed_rounded,
                        title: 'GPS / RTK-Detail',
                        subtitle:
                            controller.connectionStatus.rtkState ??
                            'GPS unbekannt',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'GPS / RTK-Detail',
                          rows: <_InfoRow>[
                            _InfoRow(
                              label: 'Qualität',
                              value:
                                  controller.connectionStatus.rtkState ??
                                  'Unbekannt',
                            ),
                            _InfoRow(
                              label: 'Satelliten',
                              value:
                                  controller.connectionStatus.gpsNumSv
                                      ?.toString() ??
                                  'Unbekannt',
                            ),
                            _InfoRow(
                              label: 'Correction age',
                              value:
                                  controller.connectionStatus.gpsDgpsAgeMs !=
                                      null
                                  ? '${controller.connectionStatus.gpsDgpsAgeMs} ms'
                                  : 'Unbekannt',
                            ),
                          ],
                        ),
                      ),
                      _ServiceEntry(
                        icon: Icons.warning_amber_rounded,
                        title: 'Fehlerprotokoll',
                        subtitle: controller.unreadNotifications == 0
                            ? 'Keine offenen Einträge'
                            : '${controller.unreadNotifications} offene Hinweise',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'Fehlerprotokoll',
                          rows: <_InfoRow>[
                            _InfoRow(
                              label: 'Offene Hinweise',
                              value: '${controller.unreadNotifications}',
                            ),
                            const _InfoRow(
                              label: 'Letzter Eintrag',
                              value: 'Keine kritischen Fehler',
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  _ServiceSection(
                    title: 'System',
                    items: <_ServiceEntry>[
                      _ServiceEntry(
                        icon: Icons.system_update_alt_rounded,
                        title: 'OTA-Update',
                        subtitle: controller.appOtaSummary,
                        onTap: () => _openAppOtaSheet(context),
                      ),
                      _ServiceEntry(
                        icon: Icons.settings_outlined,
                        title: 'Einstellungen',
                        subtitle: 'App-Version, aktiver Roboter und Hinweise',
                        onTap: () => context.push('/settings'),
                      ),
                      _ServiceEntry(
                        icon: Icons.info_outline_rounded,
                        title: 'Über Alfred',
                        subtitle: 'Versionen, Karte und Missionskontext',
                        onTap: () => _openInfoSheet(
                          context,
                          title: 'Über Alfred',
                          rows: <_InfoRow>[
                            _InfoRow(
                              label: 'Robot',
                              value: controller.robotName,
                            ),
                            _InfoRow(
                              label: 'Missionen',
                              value: '${controller.missions.length}',
                            ),
                            _InfoRow(
                              label: 'Zonen',
                              value: '${controller.zoneCount}',
                            ),
                          ],
                        ),
                      ),
                      _ServiceEntry(
                        icon: Icons.link_off_rounded,
                        title: 'Roboter vergessen',
                        subtitle:
                            'Zurück zur Discovery und lokalen Zustand leeren',
                        onTap: () async {
                          await controller.forgetRobot();
                          if (!context.mounted) {
                            return;
                          }
                          context.go('/discover');
                        },
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _openRobotStateSheet(BuildContext context) {
    final controller = AppScope.read(context);
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
                  'Roboterstatus',
                  style: Theme.of(sheetContext).textTheme.titleMedium,
                ),
                const SizedBox(height: 12),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: <Widget>[
                    ActionChip(
                      label: const Text('Lädt'),
                      onPressed: () {
                        controller.setRobotMode(RobotMode.charging);
                        Navigator.of(sheetContext).pop();
                      },
                    ),
                    ActionChip(
                      label: const Text('Geparkt'),
                      onPressed: () {
                        controller.setRobotMode(RobotMode.parking);
                        Navigator.of(sheetContext).pop();
                      },
                    ),
                    ActionChip(
                      label: const Text('Mäht'),
                      onPressed: () {
                        controller.setRobotMode(RobotMode.mowing);
                        Navigator.of(sheetContext).pop();
                      },
                    ),
                    ActionChip(
                      label: const Text('Regenpause'),
                      onPressed: () {
                        controller.setRobotMode(RobotMode.delayed);
                        Navigator.of(sheetContext).pop();
                      },
                    ),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _openInfoSheet(
    BuildContext context, {
    required String title,
    required List<_InfoRow> rows,
  }) {
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
                  title,
                  style: Theme.of(sheetContext).textTheme.titleMedium,
                ),
                const SizedBox(height: 12),
                ...rows.map(
                  (row) => Padding(
                    padding: const EdgeInsets.only(bottom: 10),
                    child: Row(
                      children: <Widget>[
                        Expanded(child: Text(row.label)),
                        Text(
                          row.value,
                          style: Theme.of(sheetContext).textTheme.bodyMedium
                              ?.copyWith(fontWeight: FontWeight.w800),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _openAppOtaSheet(BuildContext context) {
    unawaited(AppScope.read(context).ensureAppUpdateInfoLoaded());
    return showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      builder: (_) => const _AppOtaSheet(),
    );
  }
}

class _AppOtaSheet extends StatelessWidget {
  const _AppOtaSheet();

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final release = controller.latestAppRelease;
    final otaState = controller.appOtaInstallState;

    Future<void> downloadAndInstall() async {
      final messenger = ScaffoldMessenger.of(context);
      final result = await controller.downloadAndInstallLatestAppUpdate();
      if (!context.mounted) {
        return;
      }
      messenger.showSnackBar(
        SnackBar(
          content: Text(
            result ?? 'APK heruntergeladen. Installer wurde gestartet.',
          ),
        ),
      );
    }

    Future<void> installDownloaded() async {
      final messenger = ScaffoldMessenger.of(context);
      final result = await controller.installDownloadedAppUpdate();
      if (!context.mounted) {
        return;
      }
      messenger.showSnackBar(
        SnackBar(
          content: Text(
            result ??
                'Installationsdialog für die heruntergeladene APK geöffnet.',
          ),
        ),
      );
    }

    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 20, 20, 28),
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                'App-OTA',
                style: Theme.of(
                  context,
                ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.w800),
              ),
              const SizedBox(height: 8),
              const Text(
                'Installiert neue Android-Builds direkt aus GitHub Releases, wie in der v1-App.',
              ),
              const SizedBox(height: 16),
              _StatusRow(
                label: 'Installiert',
                value:
                    controller.installedAppVersion ??
                    (controller.isLoadingAppUpdateInfo
                        ? 'Wird geladen...'
                        : AppConstants.appVersionFallback),
              ),
              _StatusRow(
                label: 'Release-Quelle',
                value: AppConstants.githubReleasesPage,
              ),
              _StatusRow(
                label: 'Neueste Version',
                value: release == null
                    ? (controller.isLoadingAppUpdateInfo
                          ? 'Prüfe GitHub Releases...'
                          : 'Noch kein Release geladen')
                    : '${release.name} (${release.tagName})',
              ),
              if (release != null)
                _StatusRow(
                  label: 'APK',
                  value: release.hasApkAsset
                      ? (controller.isAppUpdateAvailable
                            ? 'Neue APK verfügbar'
                            : 'APK vorhanden, aber kein neueres Update')
                      : 'Kein APK-Asset im Release',
                ),
              if (controller.appUpdateInfoError != null) ...<Widget>[
                const SizedBox(height: 12),
                Text(
                  controller.appUpdateInfoError!,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: Theme.of(context).colorScheme.error,
                  ),
                ),
              ],
              if (otaState.isDownloading) ...<Widget>[
                const SizedBox(height: 16),
                LinearProgressIndicator(value: otaState.progress),
                const SizedBox(height: 8),
                Text(
                  otaState.progress == null
                      ? (otaState.statusMessage ?? 'Lade APK...')
                      : '${otaState.statusMessage ?? 'Lade APK...'} ${(otaState.progress! * 100).round()}%',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
              if (otaState.statusMessage != null &&
                  !otaState.isDownloading) ...<Widget>[
                const SizedBox(height: 12),
                Text(
                  otaState.statusMessage!,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: const Color(0xFF2F7D4A),
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ],
              if (otaState.errorMessage != null) ...<Widget>[
                const SizedBox(height: 12),
                Text(
                  otaState.errorMessage!,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: Theme.of(context).colorScheme.error,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ],
              if (release?.notes?.trim().isNotEmpty == true) ...<Widget>[
                const SizedBox(height: 16),
                Text(
                  'Release Notes',
                  style: Theme.of(
                    context,
                  ).textTheme.titleSmall?.copyWith(fontWeight: FontWeight.w800),
                ),
                const SizedBox(height: 8),
                Text(
                  release!.notes!.trim(),
                  maxLines: 8,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
              const SizedBox(height: 18),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: <Widget>[
                  OutlinedButton.icon(
                    onPressed: controller.isLoadingAppUpdateInfo
                        ? null
                        : () =>
                              controller.ensureAppUpdateInfoLoaded(force: true),
                    icon: const Icon(Icons.refresh_rounded),
                    label: const Text('Erneut prüfen'),
                  ),
                  if (!otaState.isDownloading &&
                      controller.isAppUpdateAvailable)
                    FilledButton.icon(
                      onPressed: downloadAndInstall,
                      icon: const Icon(Icons.system_update_alt_rounded),
                      label: const Text('Update herunterladen'),
                    ),
                  if (!otaState.isDownloading &&
                      otaState.downloadedFilePath != null)
                    OutlinedButton.icon(
                      onPressed: installDownloaded,
                      icon: const Icon(Icons.install_mobile_rounded),
                      label: const Text('Heruntergeladene APK installieren'),
                    ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _StatusRow extends StatelessWidget {
  const _StatusRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          SizedBox(
            width: 118,
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(
            child: Text(
              value,
              style: Theme.of(
                context,
              ).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700),
            ),
          ),
        ],
      ),
    );
  }
}

class _ServiceSection extends StatelessWidget {
  const _ServiceSection({required this.title, required this.items});

  final String title;
  final List<_ServiceEntry> items;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: const Color(0xFFDCE4D8)),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(18, 18, 18, 8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              title,
              style: Theme.of(
                context,
              ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 12),
            ...items.map((item) => _ServiceTile(entry: item)),
          ],
        ),
      ),
    );
  }
}

class _ServiceTile extends StatelessWidget {
  const _ServiceTile({required this.entry});

  final _ServiceEntry entry;

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: entry.onTap,
      borderRadius: BorderRadius.circular(18),
      child: Padding(
        padding: const EdgeInsets.only(bottom: 10),
        child: Row(
          children: <Widget>[
            Container(
              width: 40,
              height: 40,
              decoration: BoxDecoration(
                color: const Color(0x142F7D4A),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(entry.icon, color: const Color(0xFF2F7D4A), size: 20),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    entry.title,
                    style: Theme.of(context).textTheme.titleSmall?.copyWith(
                      color: const Color(0xFF1F2A22),
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const SizedBox(height: 3),
                  Text(
                    entry.subtitle,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: const Color(0xFF667267),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 8),
            const Icon(Icons.chevron_right_rounded, color: Color(0xFF667267)),
          ],
        ),
      ),
    );
  }
}

class _ServiceEntry {
  const _ServiceEntry({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;
}

class _InfoRow {
  const _InfoRow({required this.label, required this.value});

  final String label;
  final String value;
}
