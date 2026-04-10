import 'package:dio/dio.dart';
import 'package:open_filex/open_filex.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

import '../../core/config/app_constants.dart';
import '../../domain/update/app_release.dart';

class OtaRepository {
  OtaRepository({
    Dio? dio,
  }) : _dio = dio ?? Dio();

  final Dio _dio;

  Future<String> currentInstalledVersion() async {
    final info = await PackageInfo.fromPlatform();
    return info.version;
  }

  Future<AppRelease?> fetchLatestAppRelease() async {
    final Response<List<dynamic>> response =
        await _dio.get<List<dynamic>>(AppConstants.githubReleasesApi);

    final List<dynamic> releases = response.data ?? <dynamic>[];
    if (releases.isEmpty) return null;

    final dynamic firstRelease = releases.first;
    if (firstRelease is! Map) return null;
    final Map<String, dynamic> latest = Map<String, dynamic>.from(firstRelease);

    final dynamic rawAssets = latest['assets'];
    final List<dynamic> assets = rawAssets is List ? rawAssets : <dynamic>[];
    String? apkDownloadUrl;
    for (final dynamic asset in assets) {
      if (asset is! Map) continue;
      final Map<String, dynamic> map = Map<String, dynamic>.from(asset);
      final String name = (map['name'] as String? ?? '').toLowerCase();
      if (name.endsWith('.apk')) {
        apkDownloadUrl = map['browser_download_url'] as String?;
        break;
      }
    }

    return AppRelease(
      tagName: latest['tag_name'] as String? ?? 'unknown',
      name: latest['name'] as String? ?? latest['tag_name'] as String? ?? 'Release',
      htmlUrl: latest['html_url'] as String? ?? AppConstants.githubReleasesPage,
      publishedAt: DateTime.tryParse(latest['published_at'] as String? ?? '') ?? DateTime.now(),
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
        if (total <= 0) return;
        onProgress?.call(received / total);
      },
      options: Options(
        responseType: ResponseType.bytes,
      ),
    );

    return filePath;
  }

  Future<OpenResult> openDownloadedApk(String filePath) {
    return OpenFilex.open(filePath, type: 'application/vnd.android.package-archive');
  }
}
