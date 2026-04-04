class AppRelease {
  const AppRelease({
    required this.tagName,
    required this.name,
    required this.htmlUrl,
    required this.publishedAt,
    this.apkDownloadUrl,
    this.notes,
  });

  final String tagName;
  final String name;
  final String htmlUrl;
  final DateTime publishedAt;
  final String? apkDownloadUrl;
  final String? notes;

  bool get hasApkAsset => apkDownloadUrl != null && apkDownloadUrl!.isNotEmpty;

  String get normalizedVersion => _normalizeVersion(tagName);

  bool isNewerThan(String currentVersion) {
    return _compareVersions(normalizedVersion, _normalizeVersion(currentVersion)) > 0;
  }

  static String _normalizeVersion(String value) {
    return value.toLowerCase().startsWith('v') ? value.substring(1) : value;
  }

  static int _compareVersions(String a, String b) {
    final aParts = a.split('.').map((part) => int.tryParse(part) ?? 0).toList(growable: false);
    final bParts = b.split('.').map((part) => int.tryParse(part) ?? 0).toList(growable: false);
    final length = aParts.length > bParts.length ? aParts.length : bParts.length;

    for (var index = 0; index < length; index += 1) {
      final left = index < aParts.length ? aParts[index] : 0;
      final right = index < bParts.length ? bParts[index] : 0;
      if (left > right) return 1;
      if (left < right) return -1;
    }
    return 0;
  }
}
