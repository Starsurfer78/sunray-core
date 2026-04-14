import 'package:flutter/material.dart';

class FakeMapZoneOverlay {
  const FakeMapZoneOverlay({
    required this.id,
    required this.label,
    required this.coverageLabel,
    required this.lastRunLabel,
    required this.left,
    required this.top,
    required this.popupLeft,
    required this.popupTop,
  });

  final String id;
  final String label;
  final String coverageLabel;
  final String lastRunLabel;
  final double left;
  final double top;
  final double popupLeft;
  final double popupTop;
}

class FakeMapCanvas extends StatelessWidget {
  const FakeMapCanvas({
    super.key,
    this.showToolbar = false,
    this.showLabels = true,
    this.showZoneLabel = true,
    this.showNoGoLabel = true,
    this.showDockLabel = true,
    this.borderRadius = 24,
    this.zones = const <FakeMapZoneOverlay>[],
    this.selectedZoneId,
    this.onZoneTap,
    this.onBackgroundTap,
  });

  final bool showToolbar;
  final bool showLabels;
  final bool showZoneLabel;
  final bool showNoGoLabel;
  final bool showDockLabel;
  final double borderRadius;
  final List<FakeMapZoneOverlay> zones;
  final String? selectedZoneId;
  final ValueChanged<FakeMapZoneOverlay>? onZoneTap;
  final VoidCallback? onBackgroundTap;

  @override
  Widget build(BuildContext context) {
    final selectedZone = zones
        .where((zone) => zone.id == selectedZoneId)
        .cast<FakeMapZoneOverlay?>()
        .firstWhere((zone) => zone != null, orElse: () => null);

    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onBackgroundTap,
      child: Container(
        decoration: BoxDecoration(
          gradient: const LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: <Color>[
              Color(0xFFE8E8EC),
              Color(0xFFDFDFE5),
              Color(0xFFF2F2F6),
            ],
          ),
          borderRadius: BorderRadius.circular(borderRadius),
        ),
        child: Stack(
          children: <Widget>[
            Positioned.fill(
              child: CustomPaint(
                painter: _MapPainter(
                  highlightSecondZone:
                      selectedZoneId != null &&
                      selectedZoneId != zones.firstOrNull?.id,
                ),
              ),
            ),
            if (showLabels && showZoneLabel)
              ...zones.map(
                (zone) => Positioned(
                  left: zone.left,
                  top: zone.top,
                  child: GestureDetector(
                    onTap: () => onZoneTap?.call(zone),
                    child: _MapBadge(
                      label: zone.label,
                      background: const Color(0xF2FFFFFF),
                      foreground: const Color(0xFF1F2A22),
                      selected: zone.id == selectedZoneId,
                    ),
                  ),
                ),
              ),
            if (showNoGoLabel)
              const Positioned(
                left: 108,
                top: 58,
                child: _MapBadge(
                  label: 'No-Go',
                  background: Color(0xFFFDE9E1),
                  foreground: Color(0xFFFF6B2B),
                ),
              ),
            if (showDockLabel)
              const Positioned(
                right: 28,
                bottom: 72,
                child: _MapBadge(
                  label: 'Station',
                  background: Color(0xF2FFFFFF),
                  foreground: Color(0xFF3A3A3C),
                ),
              ),
            if (selectedZone != null)
              Positioned(
                left: selectedZone.popupLeft,
                top: selectedZone.popupTop,
                child: _ZonePopupCard(zone: selectedZone),
              ),
            if (showToolbar)
              Positioned(
                right: 16,
                top: 18,
                child: Column(
                  children: const <Widget>[
                    _MapTool(label: 'Mehr'),
                    SizedBox(height: 10),
                    _MapTool(label: 'Bearbeiten'),
                    SizedBox(height: 10),
                    _MapTool(icon: Icons.layers_outlined),
                  ],
                ),
              ),
          ],
        ),
      ),
    );
  }
}

class _MapBadge extends StatelessWidget {
  const _MapBadge({
    required this.label,
    required this.background,
    required this.foreground,
    this.selected = false,
  });

  final String label;
  final Color background;
  final Color foreground;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: selected ? const Color(0xFFFF6B2B) : background,
        borderRadius: BorderRadius.circular(12),
        boxShadow: const <BoxShadow>[
          BoxShadow(
            color: Color(0x14000000),
            blurRadius: 10,
            offset: Offset(0, 3),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Text(
          label,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            color: selected ? Colors.white : foreground,
            fontWeight: FontWeight.w800,
          ),
        ),
      ),
    );
  }
}

class _ZonePopupCard extends StatelessWidget {
  const _ZonePopupCard({required this.zone});

  final FakeMapZoneOverlay zone;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xF7FFFFFF),
        borderRadius: BorderRadius.circular(16),
        boxShadow: const <BoxShadow>[
          BoxShadow(
            color: Color(0x16000000),
            blurRadius: 18,
            offset: Offset(0, 6),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 220),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              Text(
                zone.label,
                style: Theme.of(context).textTheme.titleSmall?.copyWith(
                  color: const Color(0xFF1C1C1E),
                  fontWeight: FontWeight.w800,
                ),
              ),
              const SizedBox(height: 6),
              Text(
                'Fläche: ${zone.coverageLabel}',
                style: Theme.of(
                  context,
                ).textTheme.bodySmall?.copyWith(color: const Color(0xFF3A3A3C)),
              ),
              const SizedBox(height: 4),
              Text(
                zone.lastRunLabel,
                style: Theme.of(
                  context,
                ).textTheme.bodySmall?.copyWith(color: const Color(0xFF6C6C70)),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _MapTool extends StatelessWidget {
  const _MapTool({this.label, this.icon});

  final String? label;
  final IconData? icon;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xF2FFFFFF),
      borderRadius: BorderRadius.circular(12),
      child: Padding(
        padding: EdgeInsets.symmetric(
          horizontal: label == null ? 12 : 16,
          vertical: 12,
        ),
        child: label == null
            ? Icon(icon, color: const Color(0xFF1F2A22), size: 20)
            : Text(label!, style: Theme.of(context).textTheme.labelLarge),
      ),
    );
  }
}

class _MapPainter extends CustomPainter {
  const _MapPainter({required this.highlightSecondZone});

  final bool highlightSecondZone;

  @override
  void paint(Canvas canvas, Size size) {
    final perimeter = Paint()
      ..color = const Color(0xFF8A8A90)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3;

    final zoneFill = Paint()
      ..color = const Color(0x66C8C8D0)
      ..style = PaintingStyle.fill;

    final highlightedZoneFill = Paint()
      ..color = const Color(0x88D8D8E0)
      ..style = PaintingStyle.fill;

    final zoneBorder = Paint()
      ..color = const Color(0xFFA0A0A8)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.5;

    final route = Paint()
      ..color = const Color(0xFF8A8A90)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3;

    final noGo = Paint()
      ..color = const Color(0x88F5A882)
      ..style = PaintingStyle.fill;

    final noGoBorder = Paint()
      ..color = const Color(0xFFFF6B2B)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;

    final outer = Path()
      ..moveTo(size.width * 0.14, size.height * 0.25)
      ..lineTo(size.width * 0.78, size.height * 0.19)
      ..lineTo(size.width * 0.86, size.height * 0.42)
      ..lineTo(size.width * 0.74, size.height * 0.78)
      ..lineTo(size.width * 0.28, size.height * 0.82)
      ..lineTo(size.width * 0.12, size.height * 0.54)
      ..close();

    final zoneA = Path()
      ..moveTo(size.width * 0.23, size.height * 0.34)
      ..lineTo(size.width * 0.45, size.height * 0.30)
      ..lineTo(size.width * 0.49, size.height * 0.50)
      ..lineTo(size.width * 0.28, size.height * 0.56)
      ..close();

    final zoneB = Path()
      ..moveTo(size.width * 0.54, size.height * 0.34)
      ..lineTo(size.width * 0.72, size.height * 0.31)
      ..lineTo(size.width * 0.76, size.height * 0.56)
      ..lineTo(size.width * 0.56, size.height * 0.58)
      ..close();

    final noGoZone = Path()
      ..addOval(
        Rect.fromCenter(
          center: Offset(size.width * 0.43, size.height * 0.63),
          width: size.width * 0.13,
          height: size.width * 0.13,
        ),
      );

    final routePath = Path()
      ..moveTo(size.width * 0.18, size.height * 0.40)
      ..quadraticBezierTo(
        size.width * 0.38,
        size.height * 0.34,
        size.width * 0.64,
        size.height * 0.38,
      )
      ..quadraticBezierTo(
        size.width * 0.75,
        size.height * 0.40,
        size.width * 0.72,
        size.height * 0.70,
      );

    canvas.drawPath(outer, perimeter);
    canvas.drawPath(
      zoneA,
      highlightSecondZone ? zoneFill : highlightedZoneFill,
    );
    canvas.drawPath(zoneA, zoneBorder);
    canvas.drawPath(
      zoneB,
      highlightSecondZone ? highlightedZoneFill : zoneFill,
    );
    canvas.drawPath(zoneB, zoneBorder);
    canvas.drawPath(noGoZone, noGo);
    canvas.drawPath(noGoZone, noGoBorder);
    canvas.drawPath(routePath, route);

    final robot = Paint()..color = Colors.white;
    final robotGlow = Paint()..color = const Color(0x55FF6B2B);

    canvas.drawCircle(
      Offset(size.width * 0.68, size.height * 0.64),
      18,
      robotGlow,
    );
    canvas.drawCircle(Offset(size.width * 0.68, size.height * 0.64), 9, robot);
  }

  @override
  bool shouldRepaint(covariant _MapPainter oldDelegate) {
    return oldDelegate.highlightSecondZone != highlightSecondZone;
  }
}

extension on List<FakeMapZoneOverlay> {
  FakeMapZoneOverlay? get firstOrNull => isEmpty ? null : first;
}
