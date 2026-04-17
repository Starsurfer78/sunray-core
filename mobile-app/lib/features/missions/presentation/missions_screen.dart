import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../domain/map/map_geometry.dart';
import '../../../domain/mission/mission.dart';
import '../../../domain/robot/robot_status.dart';
import '../../../domain/robot/saved_robot.dart';
import '../../../shared/providers/app_providers.dart';
import '../../../shared/widgets/connection_notice.dart';
import '../../../shared/widgets/robot_map_view.dart';
import '../../../shared/widgets/section_card.dart';

class MissionsScreen extends ConsumerStatefulWidget {
  const MissionsScreen({super.key});

  @override
  ConsumerState<MissionsScreen> createState() => _MissionsScreenState();
}

class _MissionsScreenState extends ConsumerState<MissionsScreen> {
  _PlanningStage _stage = _PlanningStage.missions;

  @override
  Widget build(BuildContext context) {
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
        title: const Text('Planen'),
        actions: <Widget>[
          IconButton(
            tooltip: 'Dashboard',
            onPressed: () => context.go('/dashboard'),
            icon: const Icon(Icons.space_dashboard_outlined),
          ),
          IconButton(
            tooltip: 'Karte bearbeiten',
            onPressed: () => context.push('/map/edit'),
            icon: const Icon(Icons.edit_location_alt_outlined),
          ),
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
          Padding(
            padding: EdgeInsets.fromLTRB(
              16,
              status.lastError == null ? 16 : 12,
              16,
              0,
            ),
            child: Row(
              children: <Widget>[
                Expanded(
                  child: FilledButton.tonalIcon(
                    onPressed: () => context.push('/map/setup'),
                    icon: const Icon(Icons.map_outlined),
                    label: const Text('Kartensetup'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: FilledButton.tonalIcon(
                    onPressed: () => context.push('/statistics'),
                    icon: const Icon(Icons.insights_outlined),
                    label: const Text('Statistik'),
                  ),
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
            child: SegmentedButton<_PlanningStage>(
              segments: const <ButtonSegment<_PlanningStage>>[
                ButtonSegment<_PlanningStage>(
                  value: _PlanningStage.missions,
                  icon: Icon(Icons.route_rounded),
                  label: Text('Missionen'),
                ),
                ButtonSegment<_PlanningStage>(
                  value: _PlanningStage.schedule,
                  icon: Icon(Icons.schedule_rounded),
                  label: Text('Zeitplan'),
                ),
                ButtonSegment<_PlanningStage>(
                  value: _PlanningStage.run,
                  icon: Icon(Icons.play_circle_outline_rounded),
                  label: Text('Starten'),
                ),
              ],
              selected: <_PlanningStage>{_stage},
              onSelectionChanged: (selection) {
                setState(() => _stage = selection.first);
              },
            ),
          ),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(
                16,
                12,
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
                                  final Mission mission = selectedMission;
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
                  stage: _stage,
                ),
              ),
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => _createMission(ref),
        label: const Text('Neue Mission'),
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

enum _PlanningStage {
  missions,
  schedule,
  run,
}

class _MissionSheet extends ConsumerStatefulWidget {
  const _MissionSheet({
    required this.map,
    required this.missions,
    required this.selectedMission,
    required this.status,
    required this.stage,
  });

  final MapGeometry map;
  final List<Mission> missions;
  final Mission? selectedMission;
  final RobotStatus status;
  final _PlanningStage stage;

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
    final stage = widget.stage;

    return SectionCard(
      title: selectedMission == null ? 'Neue Mission' : selectedMission.name,
      icon: Icons.route_rounded,
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
                _MissionHeadline(
                  nameField: TextField(
                    controller: _nameController,
                    onChanged: (value) => _updateMission(
                      selectedMission.copyWith(name: value),
                    ),
                    decoration: const InputDecoration(
                      labelText: 'Name',
                    ),
                  ),
                  mission: selectedMission,
                ),
                const SizedBox(height: 12),
                if (stage == _PlanningStage.missions)
                  SectionCard(
                    title: 'Zonen',
                    icon: Icons.crop_square_rounded,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        Text(
                          'Wähle die Flächen, die diese Mission abdecken soll. Die Vorschau nutzt genau diese Auswahl.',
                          style: Theme.of(context).textTheme.bodySmall,
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
                                      final nextZoneIds = <String>[...selectedMission.zoneIds];
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
                                .toList(growable: false),
                          ),
                      ],
                    ),
                  ),
                if (stage == _PlanningStage.missions) const SizedBox(height: 12),
                if (stage == _PlanningStage.schedule)
                  SectionCard(
                    title: 'Zeitplan',
                    icon: Icons.schedule_rounded,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        _ScheduleOverview(
                          mission: selectedMission,
                          onEdit: () => _openScheduleSheet(context, selectedMission),
                        ),
                        if (!selectedMission.isRecurring) ...<Widget>[
                          const SizedBox(height: 12),
                          Row(
                            children: <Widget>[
                              Expanded(
                                child: FilledButton.tonalIcon(
                                  onPressed: () => _openScheduleSheet(context, selectedMission),
                                  icon: const Icon(Icons.edit_calendar_outlined),
                                  label: const Text('Zeitfenster anlegen'),
                                ),
                              ),
                            ],
                          ),
                        ],
                      ],
                    ),
                  ),
                if (stage == _PlanningStage.schedule) const SizedBox(height: 12),
                if (stage == _PlanningStage.run)
                  SectionCard(
                    title: 'Aktionen',
                    icon: Icons.play_circle_outline_rounded,
                    child: Column(
                      children: <Widget>[
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
                  ),
                if (stage == _PlanningStage.run) const SizedBox(height: 12),
                SectionCard(
                  title: 'Missionen',
                  icon: Icons.view_carousel_outlined,
                  child: SizedBox(
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

  Future<void> _openScheduleSheet(
    BuildContext context,
    Mission mission,
  ) async {
    var scheduleDays = List<bool>.from(
      mission.scheduleDays.length == 7
          ? mission.scheduleDays
          : List<bool>.filled(7, false),
    );
    var scheduleHour = mission.scheduleHour ?? 9;
    var scheduleMinute = mission.scheduleMinute ?? 0;
    var scheduleEndHour = mission.scheduleEndHour ?? 12;
    var scheduleEndMinute = mission.scheduleEndMinute ?? 0;
    var pattern = mission.pattern;
    var onlyWhenDry = mission.onlyWhenDry;
    var requiresHighBattery = mission.requiresHighBattery;
    var selectedZoneIds = List<String>.from(mission.zoneIds);
    var selectedZoneNames = List<String>.from(mission.zoneNames);

    await showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (sheetContext) => StatefulBuilder(
        builder: (sheetContext, setSheetState) => SafeArea(
          child: Padding(
            padding: EdgeInsets.fromLTRB(
              20,
              4,
              20,
              20 + MediaQuery.of(sheetContext).viewInsets.bottom,
            ),
            child: SingleChildScrollView(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisSize: MainAxisSize.min,
                children: <Widget>[
                  Text(
                    mission.isRecurring ? 'Zeitfenster bearbeiten' : 'Zeitplan anlegen',
                    style: Theme.of(sheetContext).textTheme.titleLarge,
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Wähle zuerst Zielbereiche, Tage und ein klares Zeitfenster für die automatische Ausführung.',
                    style: Theme.of(sheetContext).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 16),
                  Text(
                    'Zielbereiche',
                    style: Theme.of(sheetContext).textTheme.titleSmall,
                  ),
                  const SizedBox(height: 8),
                  if (widget.map.zones.isEmpty)
                    Text(
                      'Noch keine Zonen vorhanden. Lege zuerst Zonen in der Karte an.',
                      style: Theme.of(sheetContext).textTheme.bodySmall,
                    )
                  else
                    Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: widget.map.zones
                          .map(
                            (zone) => FilterChip(
                              label: Text(zone.name),
                              selected: selectedZoneIds.contains(zone.id),
                              onSelected: (selected) {
                                setSheetState(() {
                                  if (selected) {
                                    if (!selectedZoneIds.contains(zone.id)) {
                                      selectedZoneIds.add(zone.id);
                                    }
                                  } else {
                                    selectedZoneIds.remove(zone.id);
                                  }
                                  selectedZoneNames = widget.map.zones
                                      .where((entry) => selectedZoneIds.contains(entry.id))
                                      .map((entry) => entry.name)
                                      .toList(growable: false);
                                });
                              },
                            ),
                          )
                          .toList(growable: false),
                    ),
                  const SizedBox(height: 16),
                  _SchedulePicker(
                    days: scheduleDays,
                    hour: scheduleHour,
                    minute: scheduleMinute,
                    endHour: scheduleEndHour,
                    endMinute: scheduleEndMinute,
                    onChanged: (days, hour, minute, endHour, endMinute) {
                      setSheetState(() {
                        scheduleDays = days;
                        scheduleHour = hour;
                        scheduleMinute = minute;
                        scheduleEndHour = endHour;
                        scheduleEndMinute = endMinute;
                      });
                    },
                  ),
                  const SizedBox(height: 16),
                  DropdownButtonFormField<String>(
                    initialValue: pattern,
                    decoration: const InputDecoration(labelText: 'Muster'),
                    items: const <DropdownMenuItem<String>>[
                      DropdownMenuItem(value: 'Streifen', child: Text('Streifen')),
                      DropdownMenuItem(value: 'Spirale', child: Text('Spirale')),
                      DropdownMenuItem(value: 'Schachbrett', child: Text('Schachbrett')),
                    ],
                    onChanged: (value) {
                      if (value == null) return;
                      setSheetState(() => pattern = value);
                    },
                  ),
                  const SizedBox(height: 12),
                  SwitchListTile.adaptive(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Nur trocken'),
                    value: onlyWhenDry,
                    onChanged: (value) => setSheetState(() => onlyWhenDry = value),
                  ),
                  SwitchListTile.adaptive(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Akku > 80%'),
                    value: requiresHighBattery,
                    onChanged: (value) => setSheetState(() => requiresHighBattery = value),
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: <Widget>[
                      if (mission.isRecurring) ...<Widget>[
                        Expanded(
                          child: TextButton.icon(
                            onPressed: () {
                              _updateMission(mission.withoutSchedule());
                              Navigator.of(sheetContext).pop();
                            },
                            icon: const Icon(Icons.delete_outline_rounded),
                            label: const Text('Löschen'),
                          ),
                        ),
                        const SizedBox(width: 12),
                      ],
                      Expanded(
                        child: OutlinedButton(
                          onPressed: () => Navigator.of(sheetContext).pop(),
                          child: const Text('Abbrechen'),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: FilledButton(
                          onPressed: widget.map.zones.isEmpty ||
                                  selectedZoneIds.isEmpty ||
                                  !scheduleDays.contains(true) ||
                                  !_isScheduleRangeValid(
                                    scheduleHour,
                                    scheduleMinute,
                                    scheduleEndHour,
                                    scheduleEndMinute,
                                  )
                              ? null
                              : () {
                            _updateMission(
                              mission.copyWith(
                                zoneIds: selectedZoneIds,
                                zoneNames: selectedZoneNames,
                                isRecurring: true,
                                scheduleDays: scheduleDays,
                                scheduleHour: scheduleHour,
                                scheduleMinute: scheduleMinute,
                                scheduleEndHour: scheduleEndHour,
                                scheduleEndMinute: scheduleEndMinute,
                                pattern: pattern,
                                onlyWhenDry: onlyWhenDry,
                                requiresHighBattery: requiresHighBattery,
                              ),
                            );
                            Navigator.of(sheetContext).pop();
                          },
                          child: const Text('Übernehmen'),
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  bool _isScheduleRangeValid(
    int startHour,
    int startMinute,
    int endHour,
    int endMinute,
  ) {
    final start = startHour * 60 + startMinute;
    final end = endHour * 60 + endMinute;
    return end > start;
  }
}

class _MissionHeadline extends StatelessWidget {
  const _MissionHeadline({
    required this.nameField,
    required this.mission,
  });

  final Widget nameField;
  final Mission mission;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        nameField,
        const SizedBox(height: 12),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: <Widget>[
            _MissionStatChip(
              icon: Icons.crop_square_rounded,
              label: '${mission.zoneIds.length} Zonen',
            ),
            _MissionStatChip(
              icon: Icons.schedule_rounded,
              label: mission.effectiveScheduleLabel,
            ),
            _MissionStatChip(
              icon: Icons.pattern_rounded,
              label: mission.pattern,
            ),
          ],
        ),
      ],
    );
  }
}

class _MissionStatChip extends StatelessWidget {
  const _MissionStatChip({
    required this.icon,
    required this.label,
  });

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: const Color(0xFF0F1829),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: const Color(0xFF1E3A5F)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Icon(icon, size: 14, color: const Color(0xFF60A5FA)),
          const SizedBox(width: 6),
          Text(label),
        ],
      ),
    );
  }
}

class _ScheduleOverview extends StatelessWidget {
  const _ScheduleOverview({
    required this.mission,
    required this.onEdit,
  });

  final Mission mission;
  final VoidCallback onEdit;

  @override
  Widget build(BuildContext context) {
    final targetLabel = mission.zoneNames.isEmpty
        ? 'Noch kein Zielbereich'
        : mission.zoneNames.length == 1
            ? mission.zoneNames.first
            : '${mission.zoneNames.length} Zonen';
    final hasSchedule = mission.isRecurring &&
        mission.scheduleHour != null &&
        mission.scheduleEndHour != null;

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: const Color(0xFF0F1829),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF1E3A5F)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Text(
            'Wochenraster',
            style: Theme.of(context).textTheme.titleSmall,
          ),
          const SizedBox(height: 6),
          Text(
            hasSchedule
                ? 'Tippe auf ein Zeitfenster, um Zielbereiche oder Zeiten zu bearbeiten.'
                : 'Noch kein Zeitfenster hinterlegt. Lege direkt einen Slot für konkrete Tage und Uhrzeiten an.',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          const SizedBox(height: 12),
          _WeeklyScheduleGrid(
            mission: mission,
            onTapSlot: onEdit,
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: <Widget>[
              _MissionStatChip(icon: Icons.route_outlined, label: targetLabel),
              if (mission.effectiveTimeRangeLabel != null)
                _MissionStatChip(
                  icon: Icons.schedule_outlined,
                  label: mission.effectiveTimeRangeLabel!,
                ),
              _MissionStatChip(icon: Icons.pattern_rounded, label: mission.pattern),
              if (mission.onlyWhenDry)
                const _MissionStatChip(icon: Icons.wb_sunny_outlined, label: 'Nur trocken'),
              if (mission.requiresHighBattery)
                const _MissionStatChip(icon: Icons.battery_6_bar_rounded, label: 'Akku > 80%'),
            ],
          ),
        ],
      ),
    );
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
    required this.endHour,
    required this.endMinute,
    required this.onChanged,
  });

  final List<bool> days;
  final int hour;
  final int minute;
  final int endHour;
  final int endMinute;
  final void Function(List<bool> days, int hour, int minute, int endHour, int endMinute) onChanged;

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
                    onChanged(next, hour, minute, endHour, endMinute);
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
              onChanged: (v) => onChanged(days, v, minute, endHour, endMinute),
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
              onChanged: (v) => onChanged(days, hour, v, endHour, endMinute),
            ),
          ],
        ),
        const SizedBox(height: 12),
        Text('Endzeit', style: theme.textTheme.titleSmall),
        const SizedBox(height: 8),
        Row(
          children: <Widget>[
            _TimeSpinner(
              value: endHour,
              min: 0,
              max: 23,
              label: 'Stunde',
              onChanged: (v) => onChanged(days, hour, minute, v, endMinute),
            ),
            const Padding(
              padding: EdgeInsets.symmetric(horizontal: 8),
              child: Text(':', style: TextStyle(fontSize: 20, fontWeight: FontWeight.w700)),
            ),
            _TimeSpinner(
              value: endMinute,
              min: 0,
              max: 55,
              step: 5,
              label: 'Minute',
              onChanged: (v) => onChanged(days, hour, minute, endHour, v),
            ),
          ],
        ),
      ],
    );
  }
}

class _WeeklyScheduleGrid extends StatelessWidget {
  const _WeeklyScheduleGrid({
    required this.mission,
    required this.onTapSlot,
  });

  final Mission mission;
  final VoidCallback onTapSlot;

  static const List<String> _dayLabels = <String>['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'];
  static const List<int> _hourMarkers = <int>[0, 6, 12, 18, 24];

  @override
  Widget build(BuildContext context) {
    final hasSchedule = mission.isRecurring &&
        mission.scheduleHour != null &&
        mission.scheduleEndHour != null;

    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: const Color(0xFF09111F),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFF1E3A5F)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              const SizedBox(width: 48),
              Expanded(
                child: Row(
                  children: _hourMarkers
                      .map(
                        (hour) => Expanded(
                          child: Align(
                            alignment: hour == 24
                                ? Alignment.centerRight
                                : Alignment.centerLeft,
                            child: Text(
                              '${hour.toString().padLeft(2, '0')}:00',
                              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                                    color: const Color(0xFF94A3B8),
                                    fontWeight: FontWeight.w600,
                                  ),
                            ),
                          ),
                        ),
                      )
                      .toList(growable: false),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          ...List<Widget>.generate(_dayLabels.length, (index) {
            final daySelected = mission.scheduleDays.length > index && mission.scheduleDays[index];
            return Padding(
              padding: EdgeInsets.only(bottom: index == _dayLabels.length - 1 ? 0 : 8),
              child: _WeeklyScheduleRow(
                label: _dayLabels[index],
                showSlot: hasSchedule && daySelected,
                startMinutes: _startMinutes,
                endMinutes: _endMinutes,
                zoneLabel: mission.zoneNames.isEmpty
                    ? 'Ziel wählen'
                    : mission.zoneNames.length == 1
                        ? mission.zoneNames.first
                        : '${mission.zoneNames.length} Zonen',
                onTapSlot: onTapSlot,
              ),
            );
          }),
        ],
      ),
    );
  }

  int get _startMinutes {
    final hour = mission.scheduleHour ?? 0;
    final minute = mission.scheduleMinute ?? 0;
    return hour * 60 + minute;
  }

  int get _endMinutes {
    final hour = mission.scheduleEndHour ?? mission.scheduleHour ?? 0;
    final minute = mission.scheduleEndMinute ?? mission.scheduleMinute ?? 0;
    return hour * 60 + minute;
  }
}

class _WeeklyScheduleRow extends StatelessWidget {
  const _WeeklyScheduleRow({
    required this.label,
    required this.showSlot,
    required this.startMinutes,
    required this.endMinutes,
    required this.zoneLabel,
    required this.onTapSlot,
  });

  final String label;
  final bool showSlot;
  final int startMinutes;
  final int endMinutes;
  final String zoneLabel;
  final VoidCallback onTapSlot;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 34,
      child: Row(
        children: <Widget>[
          SizedBox(
            width: 40,
            child: Text(
              label,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: const Color(0xFFCBD5E1),
                    fontWeight: FontWeight.w600,
                  ),
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) {
                final width = constraints.maxWidth;
                final gridColor = Theme.of(context).dividerColor.withValues(alpha: 0.45);
                final left = width * (startMinutes / 1440).clamp(0.0, 1.0);
                final slotWidth = width * ((endMinutes - startMinutes) / 1440).clamp(0.06, 1.0);

                return Stack(
                  children: <Widget>[
                    Positioned.fill(
                      child: DecoratedBox(
                        decoration: BoxDecoration(
                          color: const Color(0xFF0F1829),
                          borderRadius: BorderRadius.circular(10),
                          border: Border.all(color: const Color(0xFF1E3A5F)),
                        ),
                      ),
                    ),
                    ...List<Widget>.generate(3, (index) {
                      return Positioned(
                        left: width * ((index + 1) / 4),
                        top: 0,
                        bottom: 0,
                        child: Container(width: 1, color: gridColor),
                      );
                    }),
                    if (showSlot)
                      Positioned(
                        left: left,
                        top: 3,
                        bottom: 3,
                        width: slotWidth,
                        child: Material(
                          color: Colors.transparent,
                          child: InkWell(
                            borderRadius: BorderRadius.circular(8),
                            onTap: onTapSlot,
                            child: Ink(
                              decoration: BoxDecoration(
                                color: const Color(0xFFDBEAFE),
                                borderRadius: BorderRadius.circular(8),
                                border: Border.all(color: const Color(0xFF93C5FD)),
                              ),
                              child: Center(
                                child: Padding(
                                  padding: const EdgeInsets.symmetric(horizontal: 6),
                                  child: Text(
                                    zoneLabel,
                                    maxLines: 1,
                                    overflow: TextOverflow.ellipsis,
                                    style: const TextStyle(
                                      color: Color(0xFF1E3A5F),
                                      fontSize: 11,
                                      fontWeight: FontWeight.w700,
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ),
                        ),
                      ),
                  ],
                );
              },
            ),
          ),
        ],
      ),
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
