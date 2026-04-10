import 'dart:convert';

import 'package:web_socket_channel/web_socket_channel.dart';

import '../../core/config/app_constants.dart';

class RobotWsClient {
  RobotWsClient();

  WebSocketChannel? _channel;

  bool get isConnected => _channel != null;

  Uri buildUri({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) {
    return Uri.parse('ws://$host:$port/ws/telemetry');
  }

  Stream<dynamic>? connect({
    required String host,
    int port = AppConstants.defaultRobotPort,
  }) {
    _channel = WebSocketChannel.connect(buildUri(host: host, port: port));
    return _channel?.stream;
  }

  void sendCommand(String command, [Map<String, dynamic>? payload]) {
    final channel = _channel;
    if (channel == null) return;
    try {
      channel.sink.add(jsonEncode(<String, dynamic>{
        'cmd': command,
        ...?payload,
      }));
    } on StateError {
      // Channel was already closed; the reconnect watchdog will handle recovery.
    }
  }

  Future<void> disconnect() async {
    await _channel?.sink.close();
    _channel = null;
  }
}
