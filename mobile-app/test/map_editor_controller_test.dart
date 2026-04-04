import 'package:flutter_test/flutter_test.dart';
import 'package:sunray_mobile/domain/map/map_geometry.dart';
import 'package:sunray_mobile/features/map_editor/logic/map_editor_controller.dart';
import 'package:sunray_mobile/features/map_editor/models/map_editor_state.dart';

void main() {
  const controller = MapEditorController();

  group('MapEditorController', () {
    test('adds a zone and updates zone count', () {
      const geometry = MapGeometry();

      final next = controller.addZone(geometry);

      expect(next.zones, hasLength(1));
      expect(next.zoneCount, 1);
      expect(next.zones.first.name, 'Zone 1');
    });

    test('adds and moves perimeter points', () {
      const state = MapEditorState(
        mode: MapEditorMode.record,
        activeObject: EditableMapObjectType.perimeter,
      );

      var geometry = const MapGeometry();
      geometry = controller.addPoint(geometry, state, const MapPoint(x: 1, y: 1));
      geometry = controller.addPoint(geometry, state, const MapPoint(x: 2, y: 2));
      geometry = controller.movePoint(geometry, state, 1, const MapPoint(x: 3, y: 4));

      expect(geometry.perimeter, hasLength(2));
      expect(geometry.perimeter[1].x, 3);
      expect(geometry.perimeter[1].y, 4);
    });

    test('inserts and deletes points on active zone', () {
      final geometry = controller.addZone(const MapGeometry()).copyWith(
        zones: const <ZoneGeometry>[
          ZoneGeometry(
            id: 'zone-1',
            name: 'Zone 1',
            points: <MapPoint>[
              MapPoint(x: 0, y: 0),
              MapPoint(x: 1, y: 0),
              MapPoint(x: 1, y: 1),
            ],
          ),
        ],
      );
      const state = MapEditorState(
        mode: MapEditorMode.edit,
        activeObject: EditableMapObjectType.zone,
        activeZoneId: 'zone-1',
      );

      final inserted = controller.insertPoint(
        geometry,
        state,
        1,
        const MapPoint(x: 0.5, y: 0),
      );
      final deleted = controller.deletePoint(inserted, state, 2);

      expect(inserted.zones.first.points, hasLength(4));
      expect(inserted.zones.first.points[1].x, 0.5);
      expect(deleted.zones.first.points, hasLength(3));
    });

    test('deletes active no-go object', () {
      final geometry = const MapGeometry().copyWith(
        noGoZones: const <List<MapPoint>>[
          <MapPoint>[
            MapPoint(x: 0, y: 0),
            MapPoint(x: 1, y: 0),
            MapPoint(x: 1, y: 1),
          ],
        ],
      );
      const state = MapEditorState(
        activeObject: EditableMapObjectType.noGo,
        activeNoGoIndex: 0,
      );

      final next = controller.deleteActiveObject(geometry, state);

      expect(next.noGoZones, isEmpty);
      expect(next.noGoCount, 0);
    });
  });
}
