import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../../../app/app_controller.dart';

class MissionDetailScreen extends StatefulWidget {
  const MissionDetailScreen({required this.missionId, super.key});

  final String missionId;

  @override
  State<MissionDetailScreen> createState() => _MissionDetailScreenState();
}

class _MissionDetailScreenState extends State<MissionDetailScreen> {
  bool _initialized = false;
  late TextEditingController _nameController;
  late bool _onlyWhenDry;
  late bool _requiresHighBattery;
  late bool _optimizeOrder;
  late String _scheduleLabel;
  late List<String> _selectedZones;

  static const List<String> _scheduleOptions = <String>[
    'Nur manuell',
    'Heute 17:00',
    'Mo / Mi / Fr 17:00',
    'Sa 09:30',
  ];

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_initialized) {
      return;
    }
    final mission = AppScope.read(context).missionById(widget.missionId);
    if (mission == null) {
      _nameController = TextEditingController(text: 'Mission');
      _onlyWhenDry = true;
      _requiresHighBattery = true;
      _optimizeOrder = true;
      _scheduleLabel = _scheduleOptions.first;
      _selectedZones = <String>[];
      _initialized = true;
      return;
    }
    _nameController = TextEditingController(text: mission.name);
    _onlyWhenDry = mission.onlyWhenDry;
    _requiresHighBattery = mission.requiresHighBattery;
    _optimizeOrder = mission.optimizeOrder;
    _scheduleLabel = mission.scheduleLabel;
    _selectedZones = List<String>.from(mission.zones);
    _initialized = true;
  }

  @override
  void dispose() {
    _nameController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.watch(context);
    final mission = controller.missionById(widget.missionId);
    final availableZones = controller.availableMissionZones;

    if (mission == null) {
      return Scaffold(
        body: SafeArea(
          child: Center(
            child: FilledButton(
              onPressed: () => context.go('/missions'),
              child: const Text('Zurück zu Missionen'),
            ),
          ),
        ),
      );
    }

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
                        context.go('/missions');
                      }
                    },
                    icon: const Icon(Icons.arrow_back_rounded),
                  ),
                  const SizedBox(width: 4),
                  Expanded(
                    child: Text(
                      'Mission bearbeiten',
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                ],
              ),
            ),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.fromLTRB(16, 16, 16, 24),
                children: <Widget>[
                  _SectionCard(
                    title: 'Mission',
                    child: TextField(
                      controller: _nameController,
                      decoration: const InputDecoration(
                        labelText: 'Missionsname',
                        border: OutlineInputBorder(),
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  _SectionCard(
                    title: 'Zonen wählen',
                    child: availableZones.isEmpty
                        ? const Text(
                            'Es sind noch keine Zonen vorhanden. Diese Mission kann die gesamte Karte mähen.',
                          )
                        : Wrap(
                            spacing: 8,
                            runSpacing: 8,
                            children: availableZones.map((zone) {
                              final selected = _selectedZones.contains(zone);
                              return FilterChip(
                                label: Text(zone),
                                selected: selected,
                                onSelected: (value) {
                                  setState(() {
                                    if (value) {
                                      _selectedZones = <String>[
                                        ..._selectedZones,
                                        zone,
                                      ];
                                    } else {
                                      _selectedZones = _selectedZones
                                          .where((item) => item != zone)
                                          .toList();
                                    }
                                  });
                                },
                              );
                            }).toList(),
                          ),
                  ),
                  const SizedBox(height: 12),
                  _SectionCard(
                    title: 'Reihenfolge',
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        SwitchListTile.adaptive(
                          contentPadding: EdgeInsets.zero,
                          title: const Text('Reihenfolge optimieren'),
                          subtitle: Text(
                            _optimizeOrder
                                ? 'Alfred priorisiert den effizienteren Ablauf.'
                                : 'Die gewählte Reihenfolge bleibt fix.',
                          ),
                          value: _optimizeOrder,
                          onChanged: (value) {
                            setState(() => _optimizeOrder = value);
                          },
                        ),
                        if (_selectedZones.isNotEmpty)
                          Wrap(
                            spacing: 8,
                            runSpacing: 8,
                            children: _orderedZones.map((zone) {
                              final index = _orderedZones.indexOf(zone) + 1;
                              return _OrderChip(number: index, label: zone);
                            }).toList(),
                          ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 12),
                  _SectionCard(
                    title: 'Zeitplan',
                    child: Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: _scheduleOptions.map((option) {
                        return ChoiceChip(
                          label: Text(option),
                          selected: option == _scheduleLabel,
                          onSelected: (_) {
                            setState(() => _scheduleLabel = option);
                          },
                        );
                      }).toList(),
                    ),
                  ),
                  const SizedBox(height: 12),
                  _SectionCard(
                    title: 'Bedingungen',
                    child: Column(
                      children: <Widget>[
                        SwitchListTile.adaptive(
                          contentPadding: EdgeInsets.zero,
                          title: const Text('Nur bei trockenem Wetter'),
                          value: _onlyWhenDry,
                          onChanged: (value) {
                            setState(() => _onlyWhenDry = value);
                          },
                        ),
                        SwitchListTile.adaptive(
                          contentPadding: EdgeInsets.zero,
                          title: const Text('Hohen Akkustand verlangen'),
                          value: _requiresHighBattery,
                          onChanged: (value) {
                            setState(() => _requiresHighBattery = value);
                          },
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 12),
                  _SectionCard(
                    title: 'Startfreigabe',
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        Text(
                          _selectedZones.isEmpty
                              ? availableZones.isEmpty
                                  ? 'Ohne Zonen startet diese Mission auf der gesamten Karte.'
                                  : 'Ohne Auswahl läuft diese Mission auf der gesamten Karte. Du kannst optional einzelne Zonen begrenzen.'
                              : 'Diese Mission ist startbereit und kann Alfred sofort übernehmen.',
                        ),
                        const SizedBox(height: 16),
                        FilledButton(
                          onPressed: _canStartMission(controller.hasMap)
                              ? () {
                                  final updatedMission = mission.copyWith(
                                    name: _nameController.text.trim().isEmpty
                                        ? mission.name
                                        : _nameController.text.trim(),
                                    zones: _orderedZones,
                                    scheduleLabel: _scheduleLabel,
                                    onlyWhenDry: _onlyWhenDry,
                                    requiresHighBattery: _requiresHighBattery,
                                    optimizeOrder: _optimizeOrder,
                                    statusLabel: 'Geplant',
                                    isEnabled: true,
                                  );
                                  controller.updateMission(updatedMission);
                                  controller.startMission(updatedMission.id);
                                  context.go('/dashboard');
                                }
                              : null,
                          style: FilledButton.styleFrom(
                            backgroundColor: const Color(0xFF2F7D4A),
                            foregroundColor: Colors.white,
                            minimumSize: const Size.fromHeight(54),
                          ),
                          child: const Text('Mission starten'),
                        ),
                        const SizedBox(height: 10),
                        TextButton(
                          onPressed: () {
                            controller.deleteMission(mission.id);
                            context.go('/missions');
                          },
                          child: const Text('Mission löschen'),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  List<String> get _orderedZones {
    if (_selectedZones.length < 2 || !_optimizeOrder) {
      return _selectedZones;
    }
    return List<String>.from(_selectedZones.reversed);
  }

  bool _canStartMission(bool hasMap) {
    return hasMap;
  }
}

class _SectionCard extends StatelessWidget {
  const _SectionCard({required this.title, required this.child});

  final String title;
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
            Text(
              title,
              style: Theme.of(
                context,
              ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 12),
            child,
          ],
        ),
      ),
    );
  }
}

class _OrderChip extends StatelessWidget {
  const _OrderChip({required this.number, required this.label});

  final int number;
  final String label;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xFFEFF2ED),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            CircleAvatar(
              radius: 12,
              backgroundColor: const Color(0xFF2F7D4A),
              child: Text(
                '$number',
                style: Theme.of(context).textTheme.labelSmall?.copyWith(
                  color: Colors.white,
                  fontWeight: FontWeight.w800,
                ),
              ),
            ),
            const SizedBox(width: 8),
            Text(
              label,
              style: Theme.of(
                context,
              ).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700),
            ),
          ],
        ),
      ),
    );
  }
}
