class Mission {
  const Mission({
    required this.id,
    required this.name,
    required this.zoneIds,
    required this.zoneNames,
    this.scheduleDays = const <bool>[false, false, false, false, false, false, false],
    this.scheduleHour,
    this.scheduleMinute,
    this.scheduleEndHour,
    this.scheduleEndMinute,
    this.scheduleLabel,
    this.isRecurring = false,
    this.onlyWhenDry = true,
    this.requiresHighBattery = false,
    this.pattern = 'Streifen',
  });

  final String id;
  final String name;
  final List<String> zoneIds;
  final List<String> zoneNames;
  final List<bool> scheduleDays;
  final int? scheduleHour;
  final int? scheduleMinute;
  final int? scheduleEndHour;
  final int? scheduleEndMinute;
  final String? scheduleLabel;
  final bool isRecurring;
  final bool onlyWhenDry;
  final bool requiresHighBattery;
  final String pattern;

  String get effectiveScheduleLabel {
    if (!isRecurring) return 'Manuell';
    if (scheduleHour == null) return scheduleLabel ?? 'Wiederkehrend';
    const dayNames = ['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'];
    final days = scheduleDays
        .asMap()
        .entries
        .where((entry) => entry.value)
        .map((entry) => dayNames[entry.key])
        .join(' ');
    final start = _formatTime(scheduleHour!, scheduleMinute ?? 0);
    final end = scheduleEndHour == null
        ? null
        : _formatTime(scheduleEndHour!, scheduleEndMinute ?? 0);
    final timeRange = end == null ? start : '$start-$end';
    return days.isEmpty ? timeRange : '$days $timeRange';
  }

  Map<String, dynamic> toJson() {
    return <String, dynamic>{
      'id': id,
      'name': name,
      'zoneIds': zoneIds,
      'zoneNames': zoneNames,
      'scheduleDays': scheduleDays,
      'scheduleHour': scheduleHour,
      'scheduleMinute': scheduleMinute,
      'scheduleEndHour': scheduleEndHour,
      'scheduleEndMinute': scheduleEndMinute,
      'scheduleLabel': effectiveScheduleLabel,
      'isRecurring': isRecurring,
      'onlyWhenDry': onlyWhenDry,
      'requiresHighBattery': requiresHighBattery,
      'pattern': pattern.toLowerCase() == 'streifen'
          ? 'stripe'
          : pattern.toLowerCase() == 'spirale'
              ? 'spiral'
              : pattern.toLowerCase(),
    };
  }

  Mission copyWith({
    String? id,
    String? name,
    List<String>? zoneIds,
    List<String>? zoneNames,
    List<bool>? scheduleDays,
    int? scheduleHour,
    int? scheduleMinute,
    int? scheduleEndHour,
    int? scheduleEndMinute,
    String? scheduleLabel,
    bool? isRecurring,
    bool? onlyWhenDry,
    bool? requiresHighBattery,
    String? pattern,
  }) {
    return Mission(
      id: id ?? this.id,
      name: name ?? this.name,
      zoneIds: zoneIds ?? this.zoneIds,
      zoneNames: zoneNames ?? this.zoneNames,
      scheduleDays: scheduleDays ?? this.scheduleDays,
      scheduleHour: scheduleHour ?? this.scheduleHour,
      scheduleMinute: scheduleMinute ?? this.scheduleMinute,
      scheduleEndHour: scheduleEndHour ?? this.scheduleEndHour,
      scheduleEndMinute: scheduleEndMinute ?? this.scheduleEndMinute,
      scheduleLabel: scheduleLabel ?? this.scheduleLabel,
      isRecurring: isRecurring ?? this.isRecurring,
      onlyWhenDry: onlyWhenDry ?? this.onlyWhenDry,
      requiresHighBattery: requiresHighBattery ?? this.requiresHighBattery,
      pattern: pattern ?? this.pattern,
    );
  }

  static String _formatTime(int hour, int minute) {
    final h = hour.toString().padLeft(2, '0');
    final m = minute.toString().padLeft(2, '0');
    return '$h:$m';
  }
}