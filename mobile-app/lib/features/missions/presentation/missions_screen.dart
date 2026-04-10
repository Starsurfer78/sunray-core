import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/mission/mission.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../domain/robot/saved_robot.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/robot_map_view.dart';
import '../../../shared/widgets/section_card.dart';

class MissionsScreen extends ConsumerWidget {
  const MissionsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final map = ref.watch(mapGeometryProvider);
    final missions = ref.watch(missionsProvider);
    final selectedMissionId = ref.watch(selectedMissionIdProvider);
    final selectedMission = missions.cast<Mission?>().firstWhere(
          (mission) => mission?.id == selectedMissionId,
          orElse: () => missions.isEmpty ? null : missions.first,
        );
    final status = ref.watch(connectionStateProvider);
    final loading = ref.watch(missionsLoadingProvider);
    final previewPoints = ref.watch(plannerPreviewPointsProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Missionen'),
        actions: <Widget>[
          if (loading)
            const Padding(
              padding: EdgeInsets.only(right: 16),
              child: Center(
                child: SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2),
                ),
              ),
            ),
        ],
      ),
      body: Column(
        children: <Widget>[
          if (status.lastError != null)
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 0),
              child: ConnectionNotice(status: status),
            ),
          Expanded(
            child: Padding(
              padding: EdgeInsets.fromLTRB(
                16,
                status.lastError == null ? 16 : 8,
                16,
                8,
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(20),
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: const Color(0xFF08101D),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(color: Theme.of(context).dividerColor),
                  ),
                  child: map.perimeter.length < 3
                      ? const Center(
                          child: Text('Noch keine Karte vorhanden'),
                        )
                      : RobotMapView(
                          map: map,
                          status: status.connectionState == ConnectionStateKind.connected ? status : null,
                          selectedZoneIds: selectedMission?.zoneIds ?? const <String>[],
                          interactive: true,
                          previewRoutePoints: previewPoints,
                          onZoneTap: selectedMission == null
                              ? null
                              : (zoneId) {
                                  final mission = selectedMission!;
                                  final nextZoneIds = <String>[...mission.zoneIds];
                                  if (nextZoneIds.contains(zoneId)) {
                                    nextZoneIds.remove(zoneId);
                                  } else {
                                    nextZoneIds.add(zoneId);
                                  }
                                  final nextZoneNames = map.zones
                                      .where((z) => nextZoneIds.contains(z.id))
                                      .map((z) => z.name)
                                      .toList(growable: false);
                                  final updated = mission.copyWith(
                                    zoneIds: nextZoneIds,
                                    zoneNames: nextZoneNames,
                                  );
                                  final missions = ref.read(missionsProvider);
                                  final next = missions
                                      .map((m) => m.id == updated.id ? updated : m)
                                      .toList(growable: false);
                                  ref.read(missionsProvider.notifier).state = next;
                                  // ignore: unawaited_futures
                                  ref.read(appStateStorageProvider).saveMissions(next);
                                },
                        ),
                ),
              ),
            ),
          ),
          ConstrainedBox(
            constraints: const BoxConstraints(maxHeight: 320),
            child: SingleChildScrollView(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                child: _MissionSheet(
                  map: map,
                  missions: missions,
                  selectedMission: selectedMission,
                  status: status,
                ),
              ),
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => _createMission(ref),
        label: const Text('Mission'),
        icon: const Icon(Icons.add_rounded),
      ),
    );
  }

  void _createMission(WidgetRef ref) {
    final missions = ref.read(missionsProvider);
    // Compute ID from max existing index to avoid collision after deletion.
    final maxIdx = missions
        .map((m) => int.tryParse(m.id.replaceFirst('mission-', '')) ?? 0)
        .fold(0, (a, b) => a > b ? a : b);
    final mission = Mission(
      id: 'mission-${maxIdx + 1}',
      name: 'Mission ${maxIdx + 1}',
      zoneIds: const <String>[],
      zoneNames: const <String>[],
      scheduleLabel: 'Manuell',
    );

    ref.read(missionsProvider.notifier).state = <Mission>[
      ...missions,
      mission,
    ];
    ref.read(selectedMissionIdProvider.notifier).state = mission.id;
  }
}

class _MissionSheet extends ConsumerStatefulWidget {
  const _MissionSheet({
    required this.map,
    required this.missions,
    required this.selectedMission,
    required this.status,
  });

  final MapGeometry map;
  final List<Mission> missions;
  final Mission? selectedMission;
  final RobotStatus status;

  @override
  ConsumerState<_MissionSheet> createState() => _MissionSheetState();
}

class _MissionSheetState extends ConsumerState<_MissionSheet> {
  late TextEditingController _nameController;

  @override
  void initState() {
    super.initState();
    _nameController = TextEditingController(text: widget.selectedMission?.name ?? '');
  }

  @override
  void didUpdateWidget(_MissionSheet oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Sync controller when the selected mission changes (different id).
    final newId = widget.selectedMission?.id;
    final oldId = oldWidget.selectedMission?.id;
    if (newId != oldId) {
      _nameController.text = widget.selectedMission?.name ?? '';
    }
  }

  @override
  void dispose() {
    _nameController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final map = widget.map;
    final missions = widget.missions;
    final selectedMission = widget.selectedMission;
    final status = widget.status;
    final isConnected = status.connectionState == ConnectionStateKind.connected;
    final controller = ref.read(robotConnectionControllerProvider);
    final activeRobot = ref.watch(activeRobotProvider);
    final previewLoading = ref.watch(plannerPreviewLoadingProvider);

    return SectionCard(
      title: selectedMission == null ? 'Neue Mission' : selectedMission.name,
      trailing: selectedMission == null
          ? null
          : DropdownButtonHideUnderline(
              child: DropdownButton<String>(
                value: selectedMission.id,
                items: missions
                    .map(
                      (mission) => DropdownMenuItem<String>(
                        value: mission.id,
                        child: Text(mission.name),
                      ),
                    )
                    .toList(),
                onChanged: (value) {
                  ref.read(selectedMissionIdProvider.notifier).state = value;
                  ref.read(plannerPreviewPointsProvider.notifier).state = const <MapPoint>[];
                },
              ),
            ),
      child: selectedMission == null
          ? const Text('Lege zuerst eine Mission an.')
          : Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                TextField(
                  controller: _nameController,
                  onChanged: (value) => _updateMission(
                    selectedMission.copyWith(name: value),
                  ),
                  decoration: const InputDecoration(
                    labelText: 'Name',
                  ),
                ),
                const SizedBox(height: 12),
                Text(
                  'Zonen',
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                const SizedBox(height: 8),
                if (map.zones.isEmpty)
                  Padding(
                    padding: const EdgeInsets.symmetric(vertical: 8),
                    child: Text(
                      'Noch keine Zonen vorhanden. Zonen in der Karte anlegen.',
                      style: Theme.of(context)
                          .textTheme
                          .bodySmall
                          ?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant),
                    ),
                  )
                else
                  Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: map.zones
                      .map(
                        (zone) => FilterChip(
                          label: Text(zone.name),
                          selected: selectedMission.zoneIds.contains(zone.id),
                          onSelected: (selected) {
                            final nextZoneIds = <String>[
                              ...selectedMission.zoneIds,
                            ];
                            if (selected) {
                              if (!nextZoneIds.contains(zone.id)) {
                                nextZoneIds.add(zone.id);
                              }
                            } else {
                              nextZoneIds.remove(zone.id);
                            }

                            final nextZoneNames = map.zones
                                .where((entry) => nextZoneIds.contains(entry.id))
                                .map((entry) => entry.name)
                                .toList(growable: false);

                            _updateMission(
                              selectedMission.copyWith(
                                zoneIds: nextZoneIds,
                                zoneNames: nextZoneNames,
                              ),
                            );
                          },
                        ),
                      )
                      .toList(),
                ),
                const SizedBox(height: 12),
                SegmentedButton<bool>(
                  segments: const <ButtonSegment<bool>>[
                    ButtonSegment<bool>(value: false, label: Text('Manuell')),
                    ButtonSegment<bool>(value: true, label: Text('Wiederkehrend')),
                  ],
                  selected: <bool>{selectedMission.isRecurring},
                  onSelectionChanged: (selection) {
                    _updateMission(
                      selectedMission.copyWith(
                        isRecurring: selection.first,
                      ),
                    );
                  },
                ),
                if (selectedMission.isRecurring) ...<Widget>[
                  const SizedBox(height: 12),
                  _SchedulePicker(
                    days: selectedMission.scheduleDays,
                    hour: selectedMission.scheduleHour ?? 9,
                    minute: selectedMission.scheduleMinute ?? 0,
                    onChanged: (days, hour, minute) => _updateMission(
                      selectedMission.copyWith(
                        scheduleDays: days,
                        scheduleHour: hour,
                        scheduleMinute: minute,
                      ),
                    ),
                  ),
                ],
                const SizedBox(height: 8),
                SwitchListTile.adaptive(
                  contentPadding: EdgeInsets.zero,
                  title: const Text('Nur trocken'),
                  value: selectedMission.onlyWhenDry,
                  onChanged: (value) => _updateMission(
                    selectedMission.copyWith(onlyWhenDry: value),
                  ),
                ),
                SwitchListTile.adaptive(
                  contentPadding: EdgeInsets.zero,
                  title: const Text('Akku > 80%'),
                  value: selectedMission.requiresHighBattery,
                  onChanged: (value) => _updateMission(
                    selectedMission.copyWith(requiresHighBattery: value),
                  ),
                ),
                DropdownButtonFormField<String>(
                  initialValue: selectedMission.pattern,
                  decoration: const InputDecoration(labelText: 'Muster'),
                  items: const <DropdownMenuItem<String>>[
                    DropdownMenuItem(value: 'Streifen', child: Text('Streifen')),
                    DropdownMenuItem(value: 'Spirale', child: Text('Spirale')),
                    DropdownMenuItem(value: 'Schachbrett', child: Text('Schachbrett')),
                  ],
                  onChanged: (value) {
                    if (value == null) return;
                    _updateMission(
                      selectedMission.copyWith(pattern: value),
                    );
                  },
                ),
                const SizedBox(height: 12),
                SizedBox(
                  height: 84,
                  child: ListView.separated(
                    scrollDirection: Axis.horizontal,
                    itemCount: missions.length,
                    separatorBuilder: (_, __) => const SizedBox(width: 8),
                    itemBuilder: (context, index) {
                      final mission = missions[index];
                      final selected = mission.id == selectedMission.id;
                      return GestureDetector(
                        onTap: () {
                          ref.read(selectedMissionIdProvider.notifier).state = mission.id;
                        },
                        child: Container(
                          width: 170,
                          padding: const EdgeInsets.all(12),
                          decoration: BoxDecoration(
                            color: selected ? const Color(0xFF111B2E) : const Color(0xFF0B1424),
                            borderRadius: BorderRadius.circular(16),
                            border: Border.all(
                              color: selected ? const Color(0xFF60A5FA) : Theme.of(context).dividerColor,
                            ),
                          ),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: <Widget>[
                              Text(
                                mission.name,
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                              ),
                              const Spacer(),
                              Text(
                                '${mission.zoneIds.length} Zonen · ${mission.effectiveScheduleLabel}',
                                style: Theme.of(context).textTheme.bodySmall,
                              ),
                            ],
                          ),
                        ),
                      );
                    },
                  ),
                ),
                const SizedBox(height: 12),
                Row(
                  children: <Widget>[
                    Expanded(
                      child: OutlinedButton.icon(
                        onPressed: !isConnected || selectedMission.zoneIds.isEmpty
                            ? null
                            : () => controller.startMowing(missionId: selectedMission.id),
                        icon: const Icon(Icons.play_arrow_rounded),
                        label: const Text('Start'),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: FilledButton.tonalIcon(
                        onPressed: !isConnected ? null : controller.emergencyStop,
                        icon: const Icon(Icons.stop_rounded),
                        label: const Text('Stop'),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                Row(
                  children: <Widget>[
                    Expanded(
                      child: OutlinedButton(
                        onPressed: !isConnected || selectedMission.zoneIds.isEmpty || previewLoading
                            ? null
                            : () => _previewMission(context, activeRobot, map, selectedMission),
                        child: Text(previewLoading ? 'Berechne...' : 'Vorschau'),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: FilledButton(
                        onPressed: () => _saveMission(context, activeRobot, selectedMission),
                        child: const Text('Speichern'),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                Align(
                  alignment: Alignment.centerRight,
                  child: TextButton.icon(
                    onPressed: () => _deleteMission(context, activeRobot, selectedMission),
                    icon: const Icon(Icons.delete_outline_rounded),
                    label: const Text('Löschen'),
                  ),
                ),
              ],
            ),
    );
  }

  void _updateMission(Mission updatedMission) {
    final missions = ref.read(missionsProvider);
    final next = missions
        .map((mission) => mission.id == updatedMission.id ? updatedMission : mission)
        .toList(growable: false);
    ref.read(missionsProvider.notifier).state = next;
    // Save is fire-and-forget here (called on every incremental change);
    // errors are non-critical because _saveMission() does a reliable save.
    // ignore: unawaited_futures
    ref.read(appStateStorageProvider).saveMissions(next);
  }

  Future<void> _saveMission(
    BuildContext context,
    SavedRobot? activeRobot,
    Mission mission,
  ) async {
    try {
      await ref.read(appStateStorageProvider).saveMissions(ref.read(missionsProvider));
      if (activeRobot != null) {
        await ref.read(missionRepositoryProvider).saveMission(
              host: activeRobot.lastHost,
              port: activeRobot.port,
              mission: mission,
            );
      }
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              activeRobot == null
                  ? 'Mission lokal gespeichert.'
                  : 'Mission auf Alfred gespeichert.',
            ),
          ),
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Mission konnte nicht gespeichert werden: $error')),
        );
      }
    }
  }

  Future<void> _deleteMission(
    BuildContext context,
    SavedRobot? activeRobot,
    Mission mission,
  ) async {
    final next = ref
        .read(missionsProvider)
        .where((entry) => entry.id != mission.id)
        .toList(growable: false);
    ref.read(missionsProvider.notifier).state = next;
    ref.read(selectedMissionIdProvider.notifier).state = next.isEmpty ? null : next.first.id;
    ref.read(plannerPreviewPointsProvider.notifier).state = const <MapPoint>[];
    await ref.read(appStateStorageProvider).saveMissions(next);

    try {
      if (activeRobot != null) {
        await ref.read(missionRepositoryProvider).deleteMission(
              host: activeRobot.lastHost,
              port: activeRobot.port,
              missionId: mission.id,
            );
      }
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Mission gelöscht.')),
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Mission konnte nicht gelöscht werden: $error')),
        );
      }
    }
  }

  Future<void> _previewMission(
    BuildContext context,
    SavedRobot? activeRobot,
    MapGeometry map,
    Mission mission,
  ) async {
    if (activeRobot == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Für die Vorschau erst mit Alfred verbinden.')),
      );
      return;
    }

    ref.read(plannerPreviewLoadingProvider.notifier).state = true;
    try {
      final mapPayload = ref.read(robotRepositoryProvider).encodeMapGeometry(map);
      final response = await ref.read(robotApiProvider).previewPlanner(
            host: activeRobot.lastHost,
            port: activeRobot.port,
            payload: <String, dynamic>{
              'map': mapPayload,
              'mission': mission.toJson(),
            },
          );
      final routes = response['routes'] as List<dynamic>? ?? const <dynamic>[];
      final first = routes.isEmpty ? null : routes.first as Map<String, dynamic>?;
      final route = first?['route'] as Map<String, dynamic>?;
      final points = (route?['points'] as List<dynamic>? ?? const <dynamic>[])
          .map((entry) => entry as Map<String, dynamic>)
          .map((entry) => entry['p'] as List<dynamic>? ?? const <dynamic>[])
          .where((point) => point.length >= 2)
          .map((point) => MapPoint(
                x: (point[0] as num).toDouble(),
                y: (point[1] as num).toDouble(),
              ))
          .toList(growable: false);
      ref.read(plannerPreviewPointsProvider.notifier).state = points;
      if (context.mounted && points.isEmpty) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Planner lieferte keine Route für diese Mission.')),
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Vorschau fehlgeschlagen: $error')),
        );
      }
    } finally {
      ref.read(plannerPreviewLoadingProvider.notifier).state = false;
    }
  }
}

// ---------------------------------------------------------------------------
// Wochenkalender-Picker: Wochentage + Uhrzeit
// ---------------------------------------------------------------------------

class _SchedulePicker extends StatelessWidget {
  const _SchedulePicker({
    required this.days,
    required this.hour,
    required this.minute,
    required this.onChanged,
  });

  final List<bool> days;
  final int hour;
  final int minute;
  final void Function(List<bool> days, int hour, int minute) onChanged;

  static const List<String> _dayLabels = <String>['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'];

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text('Wochentage', style: theme.textTheme.titleSmall),
        const SizedBox(height: 8),
        Row(
          children: List<Widget>.generate(7, (i) {
            final selected = i < days.length && days[i];
            return Expanded(
              child: Padding(
                padding: EdgeInsets.only(right: i < 6 ? 4 : 0),
                child: GestureDetector(
                  onTap: () {
                    final next = List<bool>.from(days.length == 7 ? days : List<bool>.filled(7, false));
                    next[i] = !next[i];
                    onChanged(next, hour, minute);
                  },
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 150),
                    padding: const EdgeInsets.symmetric(vertical: 8),
                    decoration: BoxDecoration(
                      color: selected ? const Color(0xFF1D4ED8) : const Color(0xFF0F1829),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                        color: selected ? const Color(0xFF3B82F6) : const Color(0xFF1E3A5F),
                      ),
                    ),
                    child: Text(
                      _dayLabels[i],
                      textAlign: TextAlign.center,
                      style: TextStyle(
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                        color: selected ? Colors.white : const Color(0xFF94A3B8),
                      ),
                    ),
                  ),
                ),
              ),
            );
          }),
        ),
        const SizedBox(height: 12),
        Text('Startzeit', style: theme.textTheme.titleSmall),
        const SizedBox(height: 8),
        Row(
          children: <Widget>[
            _TimeSpinner(
              value: hour,
              min: 0,
              max: 23,
              label: 'Stunde',
              onChanged: (v) => onChanged(days, v, minute),
            ),
            const Padding(
              padding: EdgeInsets.symmetric(horizontal: 8),
              child: Text(':', style: TextStyle(fontSize: 20, fontWeight: FontWeight.w700)),
            ),
            _TimeSpinner(
              value: minute,
              min: 0,
              max: 55,
              step: 5,
              label: 'Minute',
              onChanged: (v) => onChanged(days, hour, v),
            ),
          ],
        ),
      ],
    );
  }
}

class _TimeSpinner extends StatelessWidget {
  const _TimeSpinner({
    required this.value,
    required this.min,
    required this.max,
    required this.label,
    required this.onChanged,
    this.step = 1,
  });

  final int value;
  final int min;
  final int max;
  final int step;
  final String label;
  final void Function(int) onChanged;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF0F1829),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: const Color(0xFF1E3A5F)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          IconButton(
            icon: const Icon(Icons.remove, size: 16),
            onPressed: value > min
                ? () => onChanged(value - step < min ? min : value - step)
                : null,
            padding: const EdgeInsets.all(6),
            constraints: const BoxConstraints(),
          ),
          SizedBox(
            width: 36,
            child: Text(
              value.toString().padLeft(2, '0'),
              textAlign: TextAlign.center,
              style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w700),
            ),
          ),
          IconButton(
            icon: const Icon(Icons.add, size: 16),
            onPressed: value < max
                ? () => onChanged(value + step > max ? max : value + step)
                : null,
            padding: const EdgeInsets.all(6),
            constraints: const BoxConstraints(),
          ),
        ],
      ),
    );
  }
}
