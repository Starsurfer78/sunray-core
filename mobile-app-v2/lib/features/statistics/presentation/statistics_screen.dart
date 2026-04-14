import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';

class StatisticsScreen extends StatefulWidget {
  const StatisticsScreen({super.key});

  @override
  State<StatisticsScreen> createState() => _StatisticsScreenState();
}

class _StatisticsScreenState extends State<StatisticsScreen> {
  List<_MowSession> _sessions = const <_MowSession>[];
  bool _loading = false;
  bool _loaded = false;
  String? _error;

  Future<void> _loadData() async {
    final controller = AppScope.read(context);
    final robot = controller.connectedRobot;
    if (robot == null) {
      return;
    }

    setState(() {
      _loading = true;
      _error = null;
    });

    try {
      final raw = await controller.robotRepository.api.getHistoryEvents(
        host: robot.lastHost,
        port: robot.port,
        limit: 200,
      );
      final events = raw.whereType<Map<String, dynamic>>().toList(
        growable: false,
      );
      setState(() {
        _sessions = _parseSessions(events);
        _loaded = true;
      });
    } catch (error) {
      setState(() => _error = 'Fehler beim Laden: $error');
    } finally {
      setState(() => _loading = false);
    }
  }

  List<_MowSession> _parseSessions(List<Map<String, dynamic>> events) {
    final sorted = List<Map<String, dynamic>>.from(events)
      ..sort((a, b) {
        final ta = a['timestamp'] as num? ?? a['time'] as num?;
        final tb = b['timestamp'] as num? ?? b['time'] as num?;
        return (ta ?? 0).compareTo(tb ?? 0);
      });

    final result = <_MowSession>[];
    num? sessionStart;

    for (final event in sorted) {
      final phase = event['phase'] as String?;
      final ts = event['timestamp'] as num? ?? event['time'] as num?;

      if (phase == 'mowing' && sessionStart == null) {
        sessionStart = ts;
      } else if (phase != 'mowing' && sessionStart != null && ts != null) {
        result.add(_MowSession(start: sessionStart.toInt(), end: ts.toInt()));
        sessionStart = null;
      }
    }

    if (sessionStart != null && sorted.isNotEmpty) {
      final lastEvent = sorted.last;
      final lastTs =
          lastEvent['timestamp'] as num? ?? lastEvent['time'] as num?;
      if (lastTs != null) {
        result.add(
          _MowSession(
            start: sessionStart.toInt(),
            end: lastTs.toInt(),
            ongoing: true,
          ),
        );
      }
    }

    return result.reversed.toList(growable: false);
  }

  int get _totalSeconds =>
      _sessions.fold(0, (sum, session) => sum + session.durationSeconds);

  String _formatDuration(int seconds) {
    if (seconds < 60) {
      return '${seconds}s';
    }
    final h = seconds ~/ 3600;
    final m = (seconds % 3600) ~/ 60;
    if (h > 0) {
      return '${h}h ${m.toString().padLeft(2, '0')}min';
    }
    return '${m}min';
  }

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final robot = controller.connectedRobot;

    return Scaffold(
      backgroundColor: const Color(0xFFF5F7F2),
      appBar: AppBar(
        title: const Text('Statistik'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Dashboard',
            onPressed: () => context.go('/dashboard'),
            icon: const Icon(Icons.space_dashboard_outlined),
          ),
          IconButton(
            tooltip: 'Neu laden',
            onPressed: robot != null && !_loading ? _loadData : null,
            icon: _loading
                ? const SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.refresh_rounded),
          ),
        ],
      ),
      body: robot == null
          ? const Center(child: Text('Kein Roboter verbunden.'))
          : !_loaded && !_loading
          ? Center(
              child: FilledButton.icon(
                onPressed: _loadData,
                icon: const Icon(Icons.download_rounded),
                label: const Text('Statistik laden'),
              ),
            )
          : _error != null
          ? Center(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Text(
                  _error!,
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ),
            )
          : ListView(
              padding: const EdgeInsets.all(16),
              children: <Widget>[
                Row(
                  children: <Widget>[
                    Expanded(
                      child: _MetricCard(
                        icon: Icons.play_circle_rounded,
                        label: 'Sitzungen',
                        value: '${_sessions.length}',
                        color: const Color(0xFF3B82F6),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: _MetricCard(
                        icon: Icons.timer_outlined,
                        label: 'Gesamtzeit',
                        value: _formatDuration(_totalSeconds),
                        color: const Color(0xFF10B981),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Mähsessions',
                  icon: Icons.grass_rounded,
                  child: _sessions.isEmpty
                      ? const Text('Keine Mähsessions im Verlauf gefunden.')
                      : Column(
                          children: <Widget>[
                            for (
                              var i = 0;
                              i < _sessions.length;
                              i += 1
                            ) ...<Widget>[
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
            style: Theme.of(context).textTheme.headlineSmall?.copyWith(
              fontWeight: FontWeight.w700,
              color: Colors.white,
            ),
          ),
          const SizedBox(height: 2),
          Text(
            label,
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: const Color(0xFF94A3B8)),
          ),
        ],
      ),
    );
  }
}

class _SessionTile extends StatelessWidget {
  const _SessionTile({
    required this.session,
    required this.index,
    required this.formatDuration,
  });

  final _MowSession session;
  final int index;
  final String Function(int seconds) formatDuration;

  @override
  Widget build(BuildContext context) {
    final start = DateTime.fromMillisecondsSinceEpoch(session.start * 1000);
    final label = _formatDate(start);

    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 12),
      child: Row(
        children: <Widget>[
          CircleAvatar(
            backgroundColor: const Color(0x142F7D4A),
            child: Text(
              '$index',
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(fontWeight: FontWeight.w800),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  label,
                  style: Theme.of(
                    context,
                  ).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 4),
                Text(
                  session.ongoing
                      ? 'Läuft noch • ${formatDuration(session.durationSeconds)}'
                      : formatDuration(session.durationSeconds),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  String _formatDate(DateTime value) {
    final day = value.day.toString().padLeft(2, '0');
    final month = value.month.toString().padLeft(2, '0');
    final year = value.year;
    final hour = value.hour.toString().padLeft(2, '0');
    final minute = value.minute.toString().padLeft(2, '0');
    return '$day.$month.$year • $hour:$minute';
  }
}

class _MowSession {
  const _MowSession({
    required this.start,
    required this.end,
    this.ongoing = false,
  });

  final int start;
  final int end;
  final bool ongoing;

  int get durationSeconds => end > start ? end - start : 0;
}
