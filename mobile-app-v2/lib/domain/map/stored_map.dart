class StoredMap {
  const StoredMap({
    required this.id,
    required this.name,
    this.createdAtMs,
    this.updatedAtMs,
  });

  final String id;
  final String name;
  final int? createdAtMs;
  final int? updatedAtMs;
}
