import 'package:flutter/material.dart';

import 'app_controller.dart';
import 'router.dart';
import 'theme.dart';

class SunrayMobileApp extends StatefulWidget {
  const SunrayMobileApp({super.key});

  @override
  State<SunrayMobileApp> createState() => _SunrayMobileAppState();
}

class _SunrayMobileAppState extends State<SunrayMobileApp> {
  late final AppController _controller;
  late final RouterConfig<Object> _router;

  @override
  void initState() {
    super.initState();
    _controller = AppController();
    _router = buildAppRouter(_controller);
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AppScope(
      controller: _controller,
      child: MaterialApp.router(
        title: 'Sunray Mobile V2',
        debugShowCheckedModeBanner: false,
        theme: buildSunrayV2Theme(),
        routerConfig: _router,
      ),
    );
  }
}
