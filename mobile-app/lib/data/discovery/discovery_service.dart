import 'dart:async';

import 'package:multicast_dns/multicast_dns.dart';

import '../../core/config/app_constants.dart';
import '../../domain/robot/discovered_robot.dart';

class DiscoveryService {
  DiscoveryService({
    MDnsClient? client,
  }) : _client = client ?? MDnsClient();

  final MDnsClient _client;

  Stream<DiscoveredRobot> discoverRobots({
    String serviceType = '_sunray._tcp.local',
  }) async* {
    await _client.start();

    try {
      final Set<String> seen = <String>{};

      await for (final PtrResourceRecord ptr in _client.lookup<PtrResourceRecord>(
        ResourceRecordQuery.serverPointer(serviceType),
      )) {
        await for (final SrvResourceRecord srv in _client.lookup<SrvResourceRecord>(
          ResourceRecordQuery.service(ptr.domainName),
        )) {
          await for (final IPAddressResourceRecord ip
              in _client.lookup<IPAddressResourceRecord>(
            ResourceRecordQuery.addressIPv4(srv.target),
          )) {
            final String host = ip.address.address;
            final String id = '${ptr.domainName}|$host|${srv.port}';
            if (!seen.add(id)) continue;

            yield DiscoveredRobot(
              id: id,
              name: _normalizeMdnsName(ptr.domainName),
              host: host,
              port: srv.port == 0 ? AppConstants.defaultRobotPort : srv.port,
              statusHint: 'mDNS',
            );
          }
        }
      }
    } finally {
      _client.stop();
    }
  }

  String _normalizeMdnsName(String raw) {
    return raw
        .replaceAll('._sunray._tcp.local', '')
        .replaceAll('.local', '')
        .trim();
  }
}

