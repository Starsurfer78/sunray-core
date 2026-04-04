import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:open_filex/open_filex.dart';

import '../../../domain/robot/robot_status.dart';
import '../../../domain/update/app_release.dart';
import '../../../domain/update/ota_install_state.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/section_card.dart';

class ServiceScreen extends ConsumerWidget {
  const ServiceScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final latestRelease = ref.watch(latestAppReleaseProvider);
    final currentVersion = ref.watch(currentAppVersionProvider);
    final otaState = ref.watch(otaInstallStateProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final connectionState = ref.watch(connectionStateProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Service'),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: <Widget>[
          ConnectionNotice(status: connectionState),
          if (connectionState.lastError != null &&
              (connectionState.connectionState == ConnectionStateKind.error ||
                  connectionState.connectionState == ConnectionStateKind.connecting))
            const SizedBox(height: 16),
          SectionCard(
            title: 'Verbundenes Geraet',
            child: activeRobot == null
                ? const Text('Aktuell ist kein Roboter aktiv verbunden.')
                : Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(activeRobot.name),
                      const SizedBox(height: 4),
                      Text(
                        '${activeRobot.lastHost}:${activeRobot.port}',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      const SizedBox(height: 8),
                      Text(
                        _connectionLabel(connectionState),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ),
          ),
          const SizedBox(height: 16),
          SectionCard(
            title: 'App',
            child: currentVersion.when(
              data: (installedVersion) => latestRelease.when(
                data: (release) => _AppUpdateCard(
                  release: release,
                  installedVersion: installedVersion,
                  otaState: otaState,
                  onDownloadAndInstall: release == null
                      ? null
                      : () => _downloadAndInstall(context, ref, release),
                  onInstallDownloaded: otaState.downloadedFilePath == null
                      ? null
                      : () => _installDownloaded(context, ref, otaState.downloadedFilePath!),
                ),
                error: (error, _) => Text('Release-Check fehlgeschlagen: $error'),
                loading: () => const Text('Pruefe GitHub Releases...'),
              ),
              error: (error, _) => Text('App-Version konnte nicht gelesen werden: $error'),
              loading: () => const Text('Lese installierte App-Version...'),
            ),
          ),
          const SizedBox(height: 16),
          const SectionCard(
            title: 'Pi / sunray-core',
            child: Text(
              'Backend-OTA, Neustart und Verbindungsstatus werden spaeter ueber die vorhandenen '
              'sunray-core Endpunkte angebunden.',
            ),
          ),
          const SizedBox(height: 16),
          const SectionCard(
            title: 'STM',
            child: Text(
              '.bin Upload, Flash und Maintenance-Status werden spaeter analog zur WebUI angebunden.',
            ),
          ),
          const SizedBox(height: 16),
          const SectionCard(
            title: 'Build-Status',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                _StatusLine(label: 'Flutter analyze', value: 'gruen'),
                _StatusLine(label: 'Discovery / Connect', value: 'V1-Basis fertig'),
                _StatusLine(label: 'Dashboard', value: 'V1-Basis fertig'),
                _StatusLine(label: 'Missionen', value: 'Start/Stop + Auswahl fertig'),
                _StatusLine(label: 'Map Editor', value: 'flutter_map + Objektbearbeitung fertig'),
                _StatusLine(label: 'APK Build', value: 'debug APK gebaut'),
              ],
            ),
          ),
        ],
      ),
    );
  }

  String _connectionLabel(RobotStatus status) {
    return switch (status.connectionState) {
      ConnectionStateKind.connected => 'Verbunden',
      ConnectionStateKind.connecting => 'Verbinde...',
      ConnectionStateKind.discovering => 'Suche...',
      ConnectionStateKind.error => status.lastError ?? 'Fehler',
      ConnectionStateKind.disconnected => 'Getrennt',
    };
  }

  Future<void> _downloadAndInstall(
    BuildContext context,
    WidgetRef ref,
    AppRelease release,
  ) async {
    ref.read(otaInstallStateProvider.notifier).state = const OtaInstallState(
      isDownloading: true,
      progress: 0,
      statusMessage: 'Lade APK herunter...',
    );

    try {
      final filePath = await ref.read(otaRepositoryProvider).downloadApk(
            release: release,
            onProgress: (progress) {
              ref.read(otaInstallStateProvider.notifier).state = OtaInstallState(
                isDownloading: true,
                progress: progress,
                statusMessage: 'Lade APK herunter...',
              );
            },
          );

      ref.read(otaInstallStateProvider.notifier).state = OtaInstallState(
        isDownloading: false,
        progress: 1,
        downloadedFilePath: filePath,
        statusMessage: 'APK heruntergeladen. Installation bereit.',
      );

      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('APK heruntergeladen. Installation wird geöffnet.')),
        );
      }

      await _installDownloaded(context, ref, filePath);
    } catch (error) {
      ref.read(otaInstallStateProvider.notifier).state = OtaInstallState(
        isDownloading: false,
        errorMessage: 'APK-Download fehlgeschlagen: $error',
      );

      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('APK-Download fehlgeschlagen: $error')),
        );
      }
    }
  }

  Future<void> _installDownloaded(
    BuildContext context,
    WidgetRef ref,
    String filePath,
  ) async {
    try {
      final OpenResult result = await ref.read(otaRepositoryProvider).openDownloadedApk(filePath);
      if (result.type != ResultType.done && context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Installer konnte nicht geöffnet werden: ${result.message}')),
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Installation konnte nicht gestartet werden: $error')),
        );
      }
    }
  }
}

class _AppUpdateCard extends StatelessWidget {
  const _AppUpdateCard({
    required this.release,
    required this.installedVersion,
    required this.otaState,
    required this.onDownloadAndInstall,
    required this.onInstallDownloaded,
  });

  final AppRelease? release;
  final String installedVersion;
  final OtaInstallState otaState;
  final VoidCallback? onDownloadAndInstall;
  final VoidCallback? onInstallDownloaded;

  @override
  Widget build(BuildContext context) {
    final isUpdateAvailable =
        release != null && release!.hasApkAsset && release!.isNewerThan(installedVersion);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text('Aktuelle App-Version: $installedVersion'),
        const SizedBox(height: 8),
        const Text('App-OTA fuer den Start ueber GitHub Releases.'),
        const SizedBox(height: 8),
        const Text(
          'Release-Quelle: https://github.com/Starsurfer78/sunray-core/releases',
        ),
        const SizedBox(height: 8),
        Text(
          release == null
              ? 'Noch kein GitHub Release gefunden.'
              : 'Neueste Version: ${release!.name} (${release!.tagName})',
        ),
        if (release != null) ...<Widget>[
          const SizedBox(height: 8),
          Text(
            release!.hasApkAsset
                ? isUpdateAvailable
                    ? 'Neue APK verfügbar.'
                    : 'APK-Asset vorhanden. App ist bereits aktuell oder gleich alt.'
                : 'Aktuell noch kein APK-Asset im Release gefunden.',
          ),
          const SizedBox(height: 12),
          _StatusLine(
            label: 'OTA-Status',
            value: isUpdateAvailable ? 'Update verfügbar' : 'Kein neueres Update',
          ),
        ],
        if (otaState.isDownloading) ...<Widget>[
          const SizedBox(height: 12),
          LinearProgressIndicator(value: otaState.progress),
          const SizedBox(height: 8),
          Text(
            otaState.progress == null
                ? (otaState.statusMessage ?? 'Lade APK...')
                : '${otaState.statusMessage ?? 'Lade APK...'} ${(otaState.progress! * 100).round()}%',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
        if (otaState.errorMessage != null) ...<Widget>[
          const SizedBox(height: 12),
          Text(
            otaState.errorMessage!,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Theme.of(context).colorScheme.error,
                ),
          ),
        ],
        if (!otaState.isDownloading && onDownloadAndInstall != null && isUpdateAvailable) ...<Widget>[
          const SizedBox(height: 12),
          FilledButton.icon(
            onPressed: onDownloadAndInstall,
            icon: const Icon(Icons.system_update_alt_rounded),
            label: const Text('Update herunterladen'),
          ),
        ],
        if (!otaState.isDownloading && onInstallDownloaded != null) ...<Widget>[
          const SizedBox(height: 8),
          OutlinedButton.icon(
            onPressed: onInstallDownloaded,
            icon: const Icon(Icons.install_mobile_rounded),
            label: const Text('Heruntergeladene APK installieren'),
          ),
        ],
      ],
    );
  }
}

class _StatusLine extends StatelessWidget {
  const _StatusLine({
    required this.label,
    required this.value,
  });

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          SizedBox(
            width: 140,
            child: Text(
              label,
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
          Expanded(
            child: Text(value),
          ),
        ],
      ),
    );
  }
}
