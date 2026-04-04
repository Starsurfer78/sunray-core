import 'package:dio/dio.dart';

import '../../core/config/app_constants.dart';

class RobotApi {
  RobotApi({
    Dio? dio,
  }) : _dio = dio ?? Dio();

  final Dio _dio;

  String baseUrl({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) {
    return '${AppConstants.defaultRobotScheme}://$host:$port';
  }

  Future<Map<String, dynamic>> getMap({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) async {
    final response = await _dio.get<Map<String, dynamic>>('${baseUrl(host: host, port: port)}/api/map');
    return response.data ?? <String, dynamic>{};
  }

  Future<void> saveMap({
    required String host,
    required Map<String, dynamic> payload,
    int port = AppConstants.defaultRobotPort,
  }) async {
    await _dio.post<void>(
      '${baseUrl(host: host, port: port)}/api/map',
      data: payload,
    );
  }

  Future<List<dynamic>> getMissions({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) async {
    final response = await _dio.get<List<dynamic>>('${baseUrl(host: host, port: port)}/api/missions');
    return response.data ?? <dynamic>[];
  }

  Future<void> createMission({
    required String host,
    required Map<String, dynamic> payload,
    int port = AppConstants.defaultRobotPort,
  }) async {
    await _dio.post<void>(
      '${baseUrl(host: host, port: port)}/api/missions',
      data: payload,
    );
  }

  Future<void> updateMission({
    required String host,
    required String missionId,
    required Map<String, dynamic> payload,
    int port = AppConstants.defaultRobotPort,
  }) async {
    await _dio.put<void>(
      '${baseUrl(host: host, port: port)}/api/missions/$missionId',
      data: payload,
    );
  }

  Future<void> deleteMission({
    required String host,
    required String missionId,
    int port = AppConstants.defaultRobotPort,
  }) async {
    await _dio.delete<void>(
      '${baseUrl(host: host, port: port)}/api/missions/$missionId',
    );
  }

  Future<Map<String, dynamic>> previewPlanner({
    required String host,
    required Map<String, dynamic> payload,
    int port = AppConstants.defaultRobotPort,
  }) async {
    final response = await _dio.post<Map<String, dynamic>>(
      '${baseUrl(host: host, port: port)}/api/planner/preview',
      data: payload,
    );
    return response.data ?? <String, dynamic>{};
  }

  Future<Map<String, dynamic>> otaCheck({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) async {
    final response = await _dio.post<Map<String, dynamic>>(
      '${baseUrl(host: host, port: port)}/api/ota/check',
    );
    return response.data ?? <String, dynamic>{};
  }

  Future<Map<String, dynamic>> otaUpdate({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) async {
    final response = await _dio.post<Map<String, dynamic>>(
      '${baseUrl(host: host, port: port)}/api/ota/update',
    );
    return response.data ?? <String, dynamic>{};
  }

  Future<Map<String, dynamic>> restartService({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) async {
    final response = await _dio.post<Map<String, dynamic>>(
      '${baseUrl(host: host, port: port)}/api/restart',
    );
    return response.data ?? <String, dynamic>{};
  }

  Future<List<dynamic>> getHistoryEvents({
    required String host,
    int port = AppConstants.defaultRobotPort,
    int limit = 30,
  }) async {
    final response = await _dio.get<Map<String, dynamic>>(
      '${baseUrl(host: host, port: port)}/api/history/events',
      queryParameters: <String, dynamic>{'limit': limit},
    );
    final data = response.data ?? <String, dynamic>{};
    return data['events'] as List<dynamic>? ?? <dynamic>[];
  }
}
