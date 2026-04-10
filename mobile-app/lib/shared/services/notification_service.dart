import 'package:flutter_local_notifications/flutter_local_notifications.dart';

/// Thin wrapper around flutter_local_notifications.
/// Call [NotificationService.instance.init()] once at app start.
class NotificationService {
  NotificationService._();

  static final NotificationService instance = NotificationService._();

  final FlutterLocalNotificationsPlugin _plugin = FlutterLocalNotificationsPlugin();
  bool _initialized = false;

  static const _channelId = 'sunray_alerts';
  static const _channelName = 'Alfred Benachrichtigungen';
  static const _channelDesc = 'Status- und Fehlermeldungen vom Mähroboter';

  Future<void> init() async {
    if (_initialized) return;
    const AndroidInitializationSettings androidSettings =
        AndroidInitializationSettings('@mipmap/ic_launcher');

    const InitializationSettings settings = InitializationSettings(
      android: androidSettings,
    );

    await _plugin.initialize(settings);

    // Request permission on Android 13+
    await _plugin
        .resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin>()
        ?.requestNotificationsPermission();

    _initialized = true;
  }

  Future<void> showAlert({
    required int id,
    required String title,
    required String body,
  }) async {
    if (!_initialized) return;
    const AndroidNotificationDetails androidDetails = AndroidNotificationDetails(
      _channelId,
      _channelName,
      channelDescription: _channelDesc,
      importance: Importance.high,
      priority: Priority.high,
    );
    const NotificationDetails details = NotificationDetails(android: androidDetails);
    await _plugin.show(id, title, body, details);
  }

  /// Cancel a previously shown notification.
  Future<void> cancel(int id) => _plugin.cancel(id);
}
