import 'package:dio/dio.dart';
import 'package:open_filex/open_filex.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

import '../../core/config/app_constants.dart';
import '../../domain/update/app_release.dart';

class OtaRepository {
  OtaRepository({Dio? dio}) : _dio = dio ?? Dio();

  final Dio _dio;

  Future<String> currentInstalledVersion() async {
    try {
      final info = await PackageInfo.fromPlatform();
      return info.version;
    } catch (_) {
      return AppConstants.appVersionFallback;
    }
  }

  Future<AppRelease?> fetchLatestAppRelease() async {
    final response = await _dio.get<List<dynamic>>(
      AppConstants.githubReleasesApi,
    );
    final releases = response.data ?? <dynamic>[];
    if (releases.isEmpty) {
      return null;
    }

    final firstRelease = releases.first;
    if (firstRelease is! Map) {
      return null;
    }
    final latest = Map<String, dynamic>.from(firstRelease);
    final rawAssets = latest['assets'];
    final assets = rawAssets is List ? rawAssets : const <dynamic>[];
    String? apkDownloadUrl;

    for (final asset in assets) {
      if (asset is! Map) {
        continue;
      }
      final map = Map<String, dynamic>.from(asset);
      final name = (map['name'] as String? ?? '').toLowerCase();
      if (name.endsWith('.apk')) {
        apkDownloadUrl = map['browser_download_url'] as String?;
        break;
      }
    }

    return AppRelease(
      tagName: latest['tag_name'] as String? ?? 'unknown',
      name:
          latest['name'] as String? ??
          latest['tag_name'] as String? ??
          'Release',
      htmlUrl: latest['html_url'] as String? ?? AppConstants.githubReleasesPage,
      publishedAt:
          DateTime.tryParse(latest['published_at'] as String? ?? '') ??
          DateTime.now(),
      apkDownloadUrl: apkDownloadUrl,
      notes: latest['body'] as String?,
    );
  }

  Future<String> downloadApk({
    required AppRelease release,
    void Function(double progress)? onProgress,
  }) async {
    final url = release.apkDownloadUrl;
    if (url == null || url.isEmpty) {
      throw Exception('Kein APK-Download im Release gefunden');
    }

    final directory = await getApplicationDocumentsDirectory();
    final filename = 'sunray-${release.normalizedVersion}.apk';
    final filePath = p.join(directory.path, filename);

    await _dio.download(
      url,
      filePath,
      onReceiveProgress: (received, total) {
        if (total <= 0) {
          return;
        }
        onProgress?.call(received / total);
      },
      options: Options(responseType: ResponseType.bytes),
    );

    return filePath;
  }

  Future<String?> openDownloadedApk(String filePath) async {
    final result = await OpenFilex.open(
      filePath,
      type: 'application/vnd.android.package-archive',
    );
    if (result.type == ResultType.done) {
      return null;
    }
    return result.message.trim().isNotEmpty
        ? result.message
        : 'Installer konnte nicht geöffnet werden.';
  }
}
