import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../domain/robot/saved_robot.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/section_card.dart';

// ---------------------------------------------------------------------------
// Statistics Screen
// ---------------------------------------------------------------------------

class StatisticsScreen extends ConsumerStatefulWidget {
  const StatisticsScreen({super.key});

  @override
  ConsumerState<StatisticsScreen> createState() => _StatisticsScreenState();
}

class _StatisticsScreenState extends ConsumerState<StatisticsScreen> {
  List<_MowSession> _sessions = const <_MowSession>[];
  bool _loading = false;
  String? _error;
  bool _loaded = false;

  Future<void> _loadData() async {
    final SavedRobot? robot = ref.read(activeRobotProvider);
    if (robot == null) return;
    setState(() {
      _loading = true;
      _error = null;
    });
    try {
      final List<dynamic> raw = await ref.read(robotApiProvider).getHistoryEvents(
            host: robot.lastHost,
            port: robot.port,
            limit: 200,
          );
      final List<Map<String, dynamic>> events = raw
          .whereType<Map<String, dynamic>>()
          .toList(growable: false);

      setState(() {
        _sessions = _parseSessions(events);
        _loaded = true;
      });
    } catch (e) {
      setState(() => _error = 'Fehler beim Laden: $e');
    } finally {
      setState(() => _loading = false);
    }
  }

  /// Parse raw history events into completed mowing sessions.
  /// A session starts when phase == 'mowing' and ends when the next different
  /// phase begins (or there are no more events).
  List<_MowSession> _parseSessions(List<Map<String, dynamic>> events) {
    // Events arrive oldest→newest; reverse for processing newest first display
    final List<Map<String, dynamic>> sorted = List<Map<String, dynamic>>.from(events)
      ..sort((a, b) {
        final num? ta = a['timestamp'] as num? ?? a['time'] as num?;
        final num? tb = b['timestamp'] as num? ?? b['time'] as num?;
        return (ta ?? 0).compareTo(tb ?? 0);
      });

    final List<_MowSession> result = <_MowSession>[];
    num? sessionStart;

    for (int i = 0; i < sorted.length; i++) {
      final Map<String, dynamic> e = sorted[i];
      final String? phase = e['phase'] as String?;
      final num? ts = e['timestamp'] as num? ?? e['time'] as num?;

      if (phase == 'mowing' && sessionStart == null) {
        sessionStart = ts;
      } else if (phase != 'mowing' && sessionStart != null && ts != null) {
        result.add(_MowSession(
          start: sessionStart.toInt(),
          end: ts.toInt(),
        ));
        sessionStart = null;
      }
    }

    // Still ongoing session (no end event)
    if (sessionStart != null) {
      final num? lastTs = sorted.isNotEmpty
          ? (sorted.last['timestamp'] as num? ?? sorted.last['time'] as num?)
          : null;
      if (lastTs != null) {
        result.add(_MowSession(
          start: sessionStart.toInt(),
          end: lastTs.toInt(),
          ongoing: true,
        ));
      }
    }

    return result.reversed.toList(growable: false); // newest first
  }

  String _formatDuration(int seconds) {
    if (seconds < 60) return '${seconds}s';
    final int h = seconds ~/ 3600;
    final int m = (seconds % 3600) ~/ 60;
    if (h > 0) return '${h}h ${m.toString().padLeft(2, '0')}min';
    return '${m}min';
  }

  int get _totalSeconds =>
      _sessions.fold(0, (sum, s) => sum + s.durationSeconds);

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final SavedRobot? robot = ref.watch(activeRobotProvider);
    final bool connected = robot != null;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Statistik'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Dashboard',
            onPressed: () => context.go('/dashboard'),
            icon: const Icon(Icons.space_dashboard_outlined),
          ),
          IconButton(
            icon: _loading
                ? const SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.refresh_rounded),
            tooltip: 'Neu laden',
            onPressed: connected && !_loading ? _loadData : null,
          ),
        ],
      ),
      body: !connected
          ? const Center(child: Text('Kein Roboter verbunden.'))
          : !_loaded && !_loading
              ? Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: <Widget>[
                      const Icon(Icons.bar_chart_rounded, size: 48, color: Colors.white24),
                      const SizedBox(height: 16),
                      const Text('Noch keine Daten geladen.'),
                      const SizedBox(height: 16),
                      FilledButton.icon(
                        onPressed: _loadData,
                        icon: const Icon(Icons.download_rounded, size: 18),
                        label: const Text('Statistik laden'),
                      ),
                    ],
                  ),
                )
              : _error != null
                  ? Center(
                      child: Padding(
                        padding: const EdgeInsets.all(24),
                        child: Text(
                          _error!,
                          style: TextStyle(color: theme.colorScheme.error),
                          textAlign: TextAlign.center,
                        ),
                      ),
                    )
                  : ListView(
                      padding: const EdgeInsets.all(16),
                      children: <Widget>[
                        // ── Summary row ──────────────────────────────────────
                        _SummaryRow(
                          sessions: _sessions.length,
                          totalSeconds: _totalSeconds,
                        ),
                        const SizedBox(height: 16),
                        // ── Session list ─────────────────────────────────────
                        SectionCard(
                          title: 'Mähsessions',
                          icon: Icons.grass_rounded,
                          child: _sessions.isEmpty
                              ? const Padding(
                                  padding: EdgeInsets.symmetric(vertical: 8),
                                  child: Text('Keine Mähsessions im Verlauf gefunden.'),
                                )
                              : Column(
                                  children: <Widget>[
                                    for (int i = 0; i < _sessions.length; i++) ...<Widget>[
                                      if (i > 0) const Divider(height: 1),
                                      _SessionTile(
                                        session: _sessions[i],
                                        index: _sessions.length - i,
                                        formatDuration: _formatDuration,
                                      ),
                                    ],
                                  ],
                                ),
                        ),
                      ],
                    ),
    );
  }
}

// ---------------------------------------------------------------------------
// Summary row: 2 metric cards
// ---------------------------------------------------------------------------

class _SummaryRow extends StatelessWidget {
  const _SummaryRow({required this.sessions, required this.totalSeconds});

  final int sessions;
  final int totalSeconds;

  String _fmtTotal(int seconds) {
    if (seconds < 60) return '${seconds}s';
    final int h = seconds ~/ 3600;
    final int m = (seconds % 3600) ~/ 60;
    if (h > 0) return '${h}h ${m.toString().padLeft(2, '0')}m';
    return '${m}m';
  }

  @override
  Widget build(BuildContext context) {
    return Row(
      children: <Widget>[
        Expanded(
          child: _MetricCard(
            icon: Icons.play_circle_rounded,
            label: 'Sessions',
            value: '$sessions',
            color: const Color(0xFF3B82F6),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: _MetricCard(
            icon: Icons.timer_outlined,
            label: 'Gesamtzeit',
            value: _fmtTotal(totalSeconds),
            color: const Color(0xFF10B981),
          ),
        ),
      ],
    );
  }
}

class _MetricCard extends StatelessWidget {
  const _MetricCard({
    required this.icon,
    required this.label,
    required this.value,
    required this.color,
  });

  final IconData icon;
  final String label;
  final String value;
  final Color color;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF0D1B2E),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: const Color(0xFF1E3A5F)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Icon(icon, size: 22, color: color),
          const SizedBox(height: 8),
          Text(
            value,
            style: theme.textTheme.headlineSmall?.copyWith(
              fontWeight: FontWeight.w700,
              color: Colors.white,
            ),
          ),
          const SizedBox(height: 2),
          Text(
            label,
            style: theme.textTheme.bodySmall?.copyWith(
              color: const Color(0xFF94A3B8),
            ),
          ),
        ],
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Individual session tile
// ---------------------------------------------------------------------------

class _SessionTile extends StatelessWidget {
  const _SessionTile({
    required this.session,
    required this.index,
    required this.formatDuration,
  });

  final _MowSession session;
  final int index;
  final String Function(int) formatDuration;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final DateTime start =
        DateTime.fromMillisecondsSinceEpoch(session.start * 1000);
    final String label = _formatDate(start);
    final String dur = formatDuration(session.durationSeconds);

    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 10),
      child: Row(
        children: <Widget>[
          Container(
            width: 32,
            height: 32,
            decoration: BoxDecoration(
              color: const Color(0xFF1E3A5F),
              borderRadius: BorderRadius.circular(8),
            ),
            alignment: Alignment.center,
            child: Text(
              '$index',
              style: theme.textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.w700,
                color: Colors.white,
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(label, style: theme.textTheme.bodyMedium),
                if (session.ongoing)
                  Text(
                    'Läuft noch',
                    style: theme.textTheme.bodySmall
                        ?.copyWith(color: const Color(0xFF10B981)),
                  ),
              ],
            ),
          ),
          Text(
            dur,
            style: theme.textTheme.bodyMedium?.copyWith(
              fontWeight: FontWeight.w600,
              color: Colors.white,
            ),
          ),
        ],
      ),
    );
  }

  String _formatDate(DateTime value) {
    final day = value.day.toString().padLeft(2, '0');
    final month = value.month.toString().padLeft(2, '0');
    final year = value.year.toString();
    final hour = value.hour.toString().padLeft(2, '0');
    final minute = value.minute.toString().padLeft(2, '0');
    return '$day.$month.$year $hour:$minute';
  }
}

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

class _MowSession {
  const _MowSession({
    required this.start,
    required this.end,
    this.ongoing = false,
  });

  final int start; // Unix seconds
  final int end;   // Unix seconds
  final bool ongoing;

  int get durationSeconds => (end - start).clamp(0, 86400);
}
