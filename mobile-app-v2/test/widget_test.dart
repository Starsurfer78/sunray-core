import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mobile_app_v2/app/app_controller.dart';
import 'package:mobile_app_v2/app/router.dart';
import 'package:mobile_app_v2/domain/robot/robot_status.dart';
import 'package:mobile_app_v2/app/theme.dart';

void main() {
  testWidgets('dashboard is the initial surface for a known robot', (
    WidgetTester tester,
  ) async {
    final controller = AppController(restoreSession: false)
      ..hasKnownRobot = true
      ..hasMap = true
      ..zoneCount = 2
      ..noGoCount = 1
      ..batteryPercent = 82;

    await tester.pumpWidget(_TestApp(controller: controller));
    await tester.pumpAndSettle();

    expect(find.text('Alfred'), findsOneWidget);
    expect(find.text('Hinterer Garten'), findsOneWidget);
    expect(find.text('Zone A • Heute 17:00'), findsOneWidget);
    expect(find.text('Mission'), findsOneWidget);
    expect(find.text('Service'), findsOneWidget);
  });

  testWidgets('dashboard quick navigation opens service', (
    WidgetTester tester,
  ) async {
    final controller = AppController(restoreSession: false)
      ..hasKnownRobot = true
      ..hasMap = true
      ..zoneCount = 2;

    await tester.pumpWidget(_TestApp(controller: controller));
    await tester.pumpAndSettle();

    await tester.tap(find.byIcon(Icons.miscellaneous_services_outlined).first);
    await tester.pumpAndSettle();

    expect(find.text('Service'), findsOneWidget);
    expect(find.text('Verbindung, Diagnose und Systempflege'), findsOneWidget);
  });

  testWidgets('dashboard quick navigation opens controller mode', (
    WidgetTester tester,
  ) async {
    final controller = AppController(restoreSession: false)
      ..hasKnownRobot = true
      ..hasMap = true
      ..zoneCount = 1;

    await tester.pumpWidget(_TestApp(controller: controller));
    await tester.pumpAndSettle();

    await tester.tap(find.text('Controller').first);
    await tester.pumpAndSettle();

    expect(find.text('Controller-Modus'), findsOneWidget);
    expect(find.textContaining('Manuelle Fahrt'), findsOneWidget);
  });

  testWidgets('controller confirms before enabling mow motor', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1280, 720);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(() {
      tester.view.resetPhysicalSize();
      tester.view.resetDevicePixelRatio();
    });

    final controller = AppController(restoreSession: false)
      ..hasKnownRobot = true
      ..hasMap = true
      ..connectionStatus = const RobotStatus(
        connectionState: ConnectionStateKind.connected,
        statePhase: 'idle',
      );

    await tester.pumpWidget(_TestApp(controller: controller));
    await tester.pumpAndSettle();

    await tester.tap(find.text('Controller').first);
    await tester.pumpAndSettle();

    await tester.ensureVisible(find.text('Mähmotor einschalten'));
    await tester.tap(find.text('Mähmotor einschalten'));
    await tester.pumpAndSettle();

    expect(find.text('Mähmotor einschalten?'), findsOneWidget);
    expect(find.text('Warnung bestätigen'), findsOneWidget);
    expect(find.text('Abbrechen'), findsOneWidget);
  });
}

class _TestApp extends StatelessWidget {
  const _TestApp({required this.controller});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    return AppScope(
      controller: controller,
      child: MaterialApp.router(
        debugShowCheckedModeBanner: false,
        theme: buildSunrayV2Theme(),
        routerConfig: buildAppRouter(controller),
      ),
    );
  }
}
