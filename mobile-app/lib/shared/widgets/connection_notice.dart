import 'package:flutter/material.dart';

import '../../domain/robot/robot_status.dart';

class ConnectionNotice extends StatelessWidget {
  const ConnectionNotice({
    required this.status,
    super.key,
  });

  final RobotStatus status;

  @override
  Widget build(BuildContext context) {
    final text = switch (status.connectionState) {
      ConnectionStateKind.error => status.lastError,
      ConnectionStateKind.connecting => status.lastError,
      _ => null,
    };
    if (text == null || text.trim().isEmpty) return const SizedBox.shrink();

    final isError = status.connectionState == ConnectionStateKind.error;
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: isError ? const Color(0x29EF4444) : const Color(0x1FF59E0B),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: isError ? const Color(0xFFF87171) : const Color(0xFFFBBF24),
        ),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Icon(
            isError ? Icons.error_outline_rounded : Icons.wifi_tethering_error_rounded,
            color: isError ? const Color(0xFFFCA5A5) : const Color(0xFFFDE68A),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              text,
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
        ],
      ),
    );
  }
}
