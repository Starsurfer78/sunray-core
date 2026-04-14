import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_app_v2/app/app_controller.dart';
import 'package:mobile_app_v2/domain/map/map_geometry.dart';
import 'package:mobile_app_v2/features/map/domain/map_editor_controller.dart';
import 'package:mobile_app_v2/features/map/domain/map_editor_state.dart';

void main() {
  const editorController = MapEditorController();

  group('MapEditorController', () {
    test('adds a zone and updates zone count', () {
      const geometry = MapGeometry();

      final next = editorController.addZone(geometry);

      expect(next.zones, hasLength(1));
      expect(next.zoneCount, 1);
      expect(next.zones.first.name, 'Zone 1');
    });

    test('inserts and deletes points on active zone', () {
      final geometry = editorController
          .addZone(const MapGeometry())
          .copyWith(
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

      final inserted = editorController.insertPoint(
        geometry,
        state,
        1,
        const MapPoint(x: 0.5, y: 0),
      );
      final deleted = editorController.deletePoint(inserted, state, 2);

      expect(inserted.zones.first.points, hasLength(4));
      expect(inserted.zones.first.points[1].x, 0.5);
      expect(deleted.zones.first.points, hasLength(3));
    });
  });

  group('AppController map editor flow', () {
    test('dock requires two points before closing and saving', () {
      final controller = AppController(restoreSession: false);

      controller.createMapObject(EditableMapObjectType.dock);
      controller.addMapPoint(const MapPoint(x: 1, y: 1));

      expect(controller.canCloseActiveMapObject(), isFalse);
      expect(controller.canSaveMap, isFalse);

      controller.addMapPoint(const MapPoint(x: 2, y: 2));

      expect(controller.canCloseActiveMapObject(), isTrue);
      controller.dispose();
    });

    test('dragging a point creates a single undo step', () {
      final controller = AppController(restoreSession: false);
      controller.applyMapGeometry(
        const MapGeometry(
          perimeter: <MapPoint>[
            MapPoint(x: 0, y: 0),
            MapPoint(x: 1, y: 1),
            MapPoint(x: 2, y: 0),
          ],
        ),
        notify: false,
      );
      controller.setActiveMapObject(
        EditableMapObjectType.perimeter,
        mode: MapEditorMode.edit,
      );

      controller.startMapPointDrag(1);
      controller.dragMapPoint(1, const MapPoint(x: 3, y: 3));
      controller.dragMapPoint(1, const MapPoint(x: 4, y: 4));
      controller.finishMapPointDrag();

      expect(controller.mapGeometry.perimeter[1].x, 4);
      expect(controller.canUndoMapEdit, isTrue);

      controller.undoMapEdit();

      expect(controller.mapGeometry.perimeter[1].x, 1);
      expect(controller.canUndoMapEdit, isFalse);
      controller.dispose();
    });

    test('updates active zone configuration', () {
      final controller = AppController(restoreSession: false);
      controller.applyMapGeometry(
        const MapGeometry(
          perimeter: <MapPoint>[
            MapPoint(x: 0, y: 0),
            MapPoint(x: 3, y: 0),
            MapPoint(x: 3, y: 3),
          ],
          dock: <MapPoint>[MapPoint(x: 0.2, y: 0.2), MapPoint(x: 0.4, y: 0.4)],
          zones: <ZoneGeometry>[
            ZoneGeometry(
              id: 'zone-1',
              name: 'Zone 1',
              points: <MapPoint>[
                MapPoint(x: 0.5, y: 0.5),
                MapPoint(x: 1.5, y: 0.5),
                MapPoint(x: 1.5, y: 1.5),
              ],
            ),
          ],
        ),
        notify: false,
      );

      controller.updateZoneConfiguration(
        'zone-1',
        name: 'Vorne',
        priority: 3,
        mowingDirection: 'Diagonal',
        mowingProfile: 'Schnell',
      );

      final zone = controller.mapGeometry.zones.single;
      expect(zone.name, 'Vorne');
      expect(zone.priority, 3);
      expect(zone.mowingDirection, 'Diagonal');
      expect(zone.mowingProfile, 'Schnell');
      controller.dispose();
    });

    test('planner preview is blocked without connected robot', () async {
      final controller = AppController(restoreSession: false);
      controller.applyMapGeometry(
        const MapGeometry(
          perimeter: <MapPoint>[
            MapPoint(x: 0, y: 0),
            MapPoint(x: 3, y: 0),
            MapPoint(x: 3, y: 3),
          ],
          dock: <MapPoint>[MapPoint(x: 0.2, y: 0.2), MapPoint(x: 0.4, y: 0.4)],
          zones: <ZoneGeometry>[
            ZoneGeometry(
              id: 'zone-1',
              name: 'Zone 1',
              points: <MapPoint>[
                MapPoint(x: 0.5, y: 0.5),
                MapPoint(x: 1.5, y: 0.5),
                MapPoint(x: 1.5, y: 1.5),
              ],
            ),
          ],
        ),
        notify: false,
      );

      final result = await controller.loadPlannerPreview();

      expect(result, contains('Verbindung'));
      expect(controller.previewRoutePoints, isEmpty);
      controller.dispose();
    });
  });
}
