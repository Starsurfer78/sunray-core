import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:open_filex/open_filex.dart';

import '../../../domain/robot/robot_status.dart';
import '../../../domain/robot/saved_robot.dart';
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
          _PiOtaCard(
            activeRobot: activeRobot,
            connectionState: connectionState,
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

// ---------------------------------------------------------------------------
// Pi / sunray-core OTA card
// ---------------------------------------------------------------------------

class _PiOtaCard extends ConsumerStatefulWidget {
  const _PiOtaCard({
    required this.activeRobot,
    required this.connectionState,
  });

  final SavedRobot? activeRobot;
  final RobotStatus connectionState;

  @override
  ConsumerState<_PiOtaCard> createState() => _PiOtaCardState();
}

class _PiOtaCardState extends ConsumerState<_PiOtaCard> {
  String? _checkStatus;
  String? _checkHash;
  String? _checkDetail;
  bool _busy = false;
  bool _waitingForRestart = false;
  bool _sawDisconnect = false;
  int _countdown = 0;
  Timer? _countdownTimer;
  String? _errorMsg;

  @override
  void dispose() {
    _countdownTimer?.cancel();
    super.dispose();
  }

  void _startCountdown(int seconds) {
    _countdown = seconds;
    _countdownTimer?.cancel();
    _countdownTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (!mounted) return;
      setState(() => _countdown -= 1);
      if (_countdown <= 0) _clearRestartState();
    });
  }

  void _clearRestartState() {
    _countdownTimer?.cancel();
    _countdownTimer = null;
    if (mounted) {
      setState(() {
        _waitingForRestart = false;
        _sawDisconnect = false;
        _busy = false;
      });
    }
  }

  Future<void> _handleCheck() async {
    final robot = widget.activeRobot;
    if (robot == null) return;
    setState(() {
      _busy = true;
      _errorMsg = null;
      _checkStatus = null;
    });
    try {
      final result = await ref.read(robotApiProvider).otaCheck(
            host: robot.lastHost,
            port: robot.port,
          );
      setState(() {
        _checkStatus = result['status'] as String?;
        _checkHash = result['hash'] as String?;
        _checkDetail = result['detail'] as String?;
      });
    } catch (e) {
      setState(() => _errorMsg = 'Prüfung fehlgeschlagen: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  Future<void> _handleUpdate() async {
    final robot = widget.activeRobot;
    if (robot == null) return;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Update installieren'),
        content: const Text('Alfred wird nach dem Update neu gestartet. Fortfahren?'),
        actions: <Widget>[
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Abbrechen')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Update')),
        ],
      ),
    );
    if (confirmed != true) return;
    setState(() {
      _busy = true;
      _errorMsg = null;
    });
    try {
      await ref.read(robotApiProvider).otaUpdate(
            host: robot.lastHost,
            port: robot.port,
          );
      setState(() {
        _waitingForRestart = true;
        _sawDisconnect = false;
      });
      _startCountdown(120);
    } catch (e) {
      setState(() {
        _errorMsg = 'Update fehlgeschlagen: $e';
        _busy = false;
      });
    }
  }

  Future<void> _handleRestart() async {
    final robot = widget.activeRobot;
    if (robot == null) return;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Service neu starten'),
        content: const Text('sunray-core wird neu gestartet. Fortfahren?'),
        actions: <Widget>[
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Abbrechen')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Neu starten')),
        ],
      ),
    );
    if (confirmed != true) return;
    setState(() {
      _busy = true;
      _errorMsg = null;
    });
    try {
      await ref.read(robotApiProvider).restartService(
            host: robot.lastHost,
            port: robot.port,
          );
      setState(() {
        _waitingForRestart = true;
        _sawDisconnect = false;
      });
      _startCountdown(30);
    } catch (e) {
      setState(() {
        _errorMsg = 'Neustart fehlgeschlagen: $e';
        _busy = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    ref.listen<RobotStatus>(connectionStateProvider, (prev, next) {
      if (!_waitingForRestart) return;
      if (next.connectionState == ConnectionStateKind.disconnected ||
          next.connectionState == ConnectionStateKind.error) {
        setState(() => _sawDisconnect = true);
      } else if (_sawDisconnect && next.connectionState == ConnectionStateKind.connected) {
        _clearRestartState();
      }
    });

    final isConnected = widget.connectionState.connectionState == ConnectionStateKind.connected;
    final piVersion = widget.connectionState.piVersion;

    return SectionCard(
      title: 'Pi / sunray-core',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              const Text('Version: '),
              Text(
                piVersion ?? (isConnected ? 'unbekannt' : '—'),
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      fontFamily: 'monospace',
                    ),
              ),
            ],
          ),
          if (_waitingForRestart) ...<Widget>[
            const SizedBox(height: 12),
            Row(
              children: <Widget>[
                const SizedBox(
                  width: 16,
                  height: 16,
                  child: CircularProgressIndicator(strokeWidth: 2),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    _sawDisconnect
                        ? 'Verbindung getrennt, warte auf Neustart… (${_countdown}s)'
                        : 'Warte auf Service-Neustart… (${_countdown}s)',
                    style: Theme.of(context)
                        .textTheme
                        .bodySmall
                        ?.copyWith(color: Colors.amber),
                  ),
                ),
              ],
            ),
          ] else ...<Widget>[
            const SizedBox(height: 12),
            if (_checkStatus != null) ...<Widget>[
              _buildCheckResult(context),
              const SizedBox(height: 12),
            ],
            if (_errorMsg != null) ...<Widget>[
              Text(
                _errorMsg!,
                style: Theme.of(context)
                    .textTheme
                    .bodySmall
                    ?.copyWith(color: Theme.of(context).colorScheme.error),
              ),
              const SizedBox(height: 12),
            ],
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: <Widget>[
                OutlinedButton.icon(
                  onPressed: isConnected && !_busy ? _handleCheck : null,
                  icon: const Icon(Icons.system_update_outlined, size: 16),
                  label: const Text('Update prüfen'),
                ),
                if (_checkStatus == 'update_available')
                  FilledButton.icon(
                    onPressed: isConnected && !_busy ? _handleUpdate : null,
                    icon: const Icon(Icons.download_rounded, size: 16),
                    label: const Text('Installieren'),
                  ),
                OutlinedButton.icon(
                  onPressed: isConnected && !_busy ? _handleRestart : null,
                  icon: const Icon(Icons.restart_alt_rounded, size: 16),
                  label: const Text('Neu starten'),
                ),
              ],
            ),
          ],
        ],
      ),
    );
  }

  Widget _buildCheckResult(BuildContext context) {
    switch (_checkStatus) {
      case 'up_to_date':
        return Row(
          children: <Widget>[
            Icon(Icons.check_circle_outline_rounded,
                size: 16, color: Colors.green.shade400),
            const SizedBox(width: 6),
            const Text('Bereits aktuell'),
          ],
        );
      case 'update_available':
        return Row(
          children: <Widget>[
            Icon(Icons.new_releases_outlined,
                size: 16, color: Colors.blue.shade300),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                'Update verfügbar${_checkHash != null ? ': ${_checkHash!.substring(0, _checkHash!.length.clamp(0, 8))}' : ''}',
              ),
            ),
          ],
        );
      default:
        return Row(
          children: <Widget>[
            Icon(Icons.error_outline_rounded,
                size: 16, color: Theme.of(context).colorScheme.error),
            const SizedBox(width: 6),
            Expanded(child: Text(_checkDetail ?? 'Fehler beim Prüfen')),
          ],
        );
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
