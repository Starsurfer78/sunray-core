class Mission {
  const Mission({
    required this.id,
    required this.name,
    required this.zoneIds,
    required this.zoneNames,
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
  final String? scheduleLabel;
  final bool isRecurring;
  final bool onlyWhenDry;
  final bool requiresHighBattery;
  final String pattern;

  Map<String, dynamic> toJson() {
    return <String, dynamic>{
      'id': id,
      'name': name,
      'zoneIds': zoneIds,
      'zoneNames': zoneNames,
      'scheduleLabel': scheduleLabel,
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
      scheduleLabel: scheduleLabel ?? this.scheduleLabel,
      isRecurring: isRecurring ?? this.isRecurring,
      onlyWhenDry: onlyWhenDry ?? this.onlyWhenDry,
      requiresHighBattery: requiresHighBattery ?? this.requiresHighBattery,
      pattern: pattern ?? this.pattern,
    );
  }
}
