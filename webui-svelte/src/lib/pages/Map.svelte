<script lang="ts">
  import { onMount } from "svelte";
  import PageLayout from "../components/PageLayout.svelte";
  import MapCanvas from "../components/Map/MapCanvas.svelte";
  import { mapGpsOrigin } from "../stores/mapGpsOrigin";
  import BottomPanel from "../components/Dashboard/BottomPanel.svelte";
  import {
    activateStoredMap,
    createStoredMap,
    deleteStoredMap,
    exportMapGeoJson,
    getMapDocument,
    getStoredMaps,
    importMapGeoJson,
    saveMapDocument,
    type MapDocument,
    type MapZone,
    type StoredMapEntry,
  } from "../api/rest";
  import { type Telemetry } from "../api/types";
  import { connection } from "../stores/connection";
  import { toast } from "../stores/notificationStore";
  import { withLoading } from "../stores/loadingState";
  import { createUndoRedo } from "../stores/undoRedo";
  import {
    mapStore,
    type ExclusionMeta,
    type MapLoadDocument,
    type MapTool,
    type Point,
    type RobotMap,
    type Zone,
  } from "../stores/map";
  import { telemetry } from "../stores/telemetry";
  import { mappingTestMode } from "../stores/mapUi";
  import {
    CONNECTION_FRESH_MS,
    normalizePoints,
    normalizeZone,
    orientation,
    segmentsIntersect,
    hasSelfIntersection,
  } from "../utils/mapHelpers";

  let busy = false;
  let info = "";
  let infoTone: "success" | "warning" | "error" = "success";
  let infoTimer: ReturnType<typeof setTimeout> | null = null;
  let mapCanvas: MapCanvas;
  let sidebarCollapsed = false;
  let nowMs = Date.now();
  let hydrationDone = false;
  let storedMaps: StoredMapEntry[] = [];
  let activeMapId = "";
  let selectedMapId = "";
  let draftAvailable = false;
  let draftSavedAt = 0;
  let mapHistory: ReturnType<typeof createUndoRedo<RobotMap>>;
  let lastHistorySnapshot = "";
  let applyingHistory = false;
  let undoLabel = "";
  let redoLabel = "";
  const SHOW_ADVANCED_MAP_TUNING = false;
  const DRAFT_KEY = "sunray-map-draft-v1";
  const GOOD_ACCURACY_M = 0.05;
  const MAX_DOCK_ENTRY_DISTANCE_M = 2.0;

  type WorkflowStepId =
    | "new"
    | "rtk"
    | "perimeter"
    | "nogo"
    | "dock"
    | "validate"
    | "save";

  type WorkflowStep = {
    id: WorkflowStepId;
    label: string;
    status: "done" | "active" | "blocked" | "pending";
    detail: string;
  };

  type DraftDocument = {
    savedAt: number;
    map: RobotMap;
  };

  type Segment = { a: Point; b: Point };

  function showInfo(
    msg: string,
    tone: "success" | "warning" | "error" = "success",
  ) {
    info = msg;
    infoTone = tone;
    if (infoTimer) clearTimeout(infoTimer);
    infoTimer = setTimeout(() => {
      info = "";
    }, 2500);

    if (tone === "error") {
      toast.error(msg, 6000);
    } else if (tone === "warning") {
      toast.warning(msg, 3000);
    } else {
      toast.success(msg, 2200);
    }
  }

  function normalizeMapDocument(map: MapDocument): MapLoadDocument {
    const perimeter = normalizePoints(map.perimeter);

    // Detect GPS (WGS84) coordinates saved by the mobile app and project them
    // to local metres using an equirectangular projection centred at the
    // perimeter centroid.  Without this the rendering pipeline (which expects
    // metres) would display a ~0.001° garden as a sub-pixel dot.
    // The GPS origin is also stored so MapCanvas can render OSM tile background.
    let projectPoints = (pts: Point[]): Point[] => pts;
    mapGpsOrigin.set(null);
    if (perimeter.length >= 3) {
      let sumX = 0;
      let sumY = 0;
      for (const p of perimeter) {
        sumX += p.x;
        sumY += p.y;
      }
      const avgX = sumX / perimeter.length;
      const avgY = sumY / perimeter.length;
      const spanX =
        Math.max(...perimeter.map((p) => p.x)) -
        Math.min(...perimeter.map((p) => p.x));
      const spanY =
        Math.max(...perimeter.map((p) => p.y)) -
        Math.min(...perimeter.map((p) => p.y));
      // A typical garden in GPS degrees spans < 0.5° and coordinates are in
      // valid lat/lon ranges.  Local-metre maps span tens of metres (> 0.5).
      if (
        avgY >= -90 &&
        avgY <= 90 &&
        avgX >= -180 &&
        avgX <= 180 &&
        spanX > 0 &&
        spanX < 0.5 &&
        spanY > 0 &&
        spanY < 0.5
      ) {
        const EARTH_R = 6378137.0;
        const toRad = Math.PI / 180;
        const cosLat = Math.cos(avgY * toRad);
        const refLon = avgX;
        const refLat = avgY;
        mapGpsOrigin.set({ lat: refLat, lon: refLon });
        projectPoints = (pts: Point[]): Point[] =>
          pts.map((p) => ({
            x: (p.x - refLon) * cosLat * EARTH_R * toRad,
            y: (p.y - refLat) * EARTH_R * toRad,
          }));
      }
    }

    const projectedPerimeter = projectPoints(perimeter);
    let dock = projectPoints(normalizePoints(map.dock));
    if (projectedPerimeter.length >= 3 && dock.length >= 2) {
      const entry = dock[0];
      const terminal = dock[dock.length - 1];
      const entryOk =
        pointInPolygon(entry, projectedPerimeter) ||
        minDistanceToPolygon(entry, projectedPerimeter) <=
          MAX_DOCK_ENTRY_DISTANCE_M;
      const terminalOk =
        pointInPolygon(terminal, projectedPerimeter) ||
        minDistanceToPolygon(terminal, projectedPerimeter) <=
          MAX_DOCK_ENTRY_DISTANCE_M;
      if (!entryOk && terminalOk) {
        dock = [...dock].reverse();
      }
    }
    return {
      perimeter: projectedPerimeter,
      dock,
      exclusions: (map.exclusions ?? []).map((e) =>
        projectPoints(normalizePoints(e as Array<[number, number]>)),
      ),
      zones: (map.zones ?? []).map((zone, index) => {
        const z = normalizeZone(zone, index);
        return { ...z, polygon: projectPoints(z.polygon) };
      }),
      planner: map.planner,
      dockMeta: map.dockMeta
        ? {
            ...map.dockMeta,
            corridor: projectPoints(normalizePoints(map.dockMeta.corridor)),
          }
        : undefined,
      exclusionMeta: map.exclusionMeta as ExclusionMeta[] | undefined,
      captureMeta: map.captureMeta ?? {},
    };
  }

  function pointInPolygon(point: Point, polygon: Point[]) {
    let inside = false;
    for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
      const xi = polygon[i].x;
      const yi = polygon[i].y;
      const xj = polygon[j].x;
      const yj = polygon[j].y;

      const intersects =
        yi > point.y !== yj > point.y &&
        point.x <
          ((xj - xi) * (point.y - yi)) / (yj - yi || Number.EPSILON) + xi;

      if (intersects) inside = !inside;
    }
    return inside;
  }

  function distanceToSegment(point: Point, segment: Segment) {
    const dx = segment.b.x - segment.a.x;
    const dy = segment.b.y - segment.a.y;
    if (dx === 0 && dy === 0) {
      return Math.hypot(point.x - segment.a.x, point.y - segment.a.y);
    }

    const t =
      ((point.x - segment.a.x) * dx + (point.y - segment.a.y) * dy) /
      (dx * dx + dy * dy);
    const clamped = Math.max(0, Math.min(1, t));
    const projection = {
      x: segment.a.x + clamped * dx,
      y: segment.a.y + clamped * dy,
    };
    return Math.hypot(point.x - projection.x, point.y - projection.y);
  }

  function minDistanceToPolygon(point: Point, polygon: Point[]) {
    if (polygon.length < 2) return Number.POSITIVE_INFINITY;
    let minDistance = Number.POSITIVE_INFINITY;
    for (let index = 0; index < polygon.length; index += 1) {
      const segment = {
        a: polygon[index],
        b: polygon[(index + 1) % polygon.length],
      };
      minDistance = Math.min(minDistance, distanceToSegment(point, segment));
    }
    return minDistance;
  }

  function mapPayload() {
    return {
      perimeter: $mapStore.map.perimeter.map((p) => [p.x, p.y]),
      dock: $mapStore.map.dock.map((p) => [p.x, p.y]),
      exclusions: $mapStore.map.exclusions.map((ex) =>
        ex.map((p) => [p.x, p.y]),
      ),
      exclusionMeta: ($mapStore.map.exclusionMeta ?? []).map((meta) => ({
        type: meta.type,
        clearance: meta.clearance,
        costScale: meta.costScale,
      })),
      zones: $mapStore.map.zones.map((zone) => ({
        ...zone,
        polygon: zone.polygon.map((p) => [p.x, p.y]),
      })),
      planner: $mapStore.map.planner,
      dockMeta: $mapStore.map.dockMeta
        ? {
            ...$mapStore.map.dockMeta,
            corridor: $mapStore.map.dockMeta.corridor.map((p) => [p.x, p.y]),
          }
        : undefined,
      captureMeta: $mapStore.map.captureMeta,
    };
  }

  function restoreHistoryState(nextMap: RobotMap) {
    applyingHistory = true;
    mapStore.restore(structuredClone(nextMap));
    lastHistorySnapshot = JSON.stringify(nextMap);
    queueMicrotask(() => {
      applyingHistory = false;
    });
  }

  function undoMapEdit() {
    if (!mapHistory || !mapHistory.canUndo()) return;
    mapHistory.undo();
    restoreHistoryState(mapHistory.getCurrentState());
    toast.info(`Rückgängig: ${undoLabel || "Letzte Änderung"}`);
  }

  function redoMapEdit() {
    if (!mapHistory || !mapHistory.canRedo()) return;
    mapHistory.redo();
    restoreHistoryState(mapHistory.getCurrentState());
    toast.info(`Wiederhergestellt: ${redoLabel || "Nächste Änderung"}`);
  }

  function handleKeydown(event: KeyboardEvent) {
    const modifier = event.ctrlKey || event.metaKey;
    if (!modifier) return;
    if (event.key.toLowerCase() !== "z") return;

    event.preventDefault();
    if (event.shiftKey) {
      redoMapEdit();
      return;
    }
    undoMapEdit();
  }

  function saveDraft() {
    if (typeof localStorage === "undefined" || !hydrationDone) return;
    const draft: DraftDocument = {
      savedAt: Date.now(),
      map: structuredClone($mapStore.map),
    };
    localStorage.setItem(DRAFT_KEY, JSON.stringify(draft));
    draftAvailable = true;
    draftSavedAt = draft.savedAt;
  }

  function clearDraft() {
    if (typeof localStorage === "undefined") return;
    localStorage.removeItem(DRAFT_KEY);
    draftAvailable = false;
    draftSavedAt = 0;
  }

  function readDraft(): DraftDocument | null {
    if (typeof localStorage === "undefined") return null;
    const raw = localStorage.getItem(DRAFT_KEY);
    if (!raw) {
      draftAvailable = false;
      draftSavedAt = 0;
      return null;
    }

    try {
      const parsed = JSON.parse(raw) as DraftDocument;
      draftAvailable = true;
      draftSavedAt = parsed.savedAt ?? 0;
      return parsed;
    } catch {
      localStorage.removeItem(DRAFT_KEY);
      draftAvailable = false;
      draftSavedAt = 0;
      return null;
    }
  }

  function restoreDraft(): boolean {
    const draft = readDraft();
    if (!draft) return false;

    mapStore.load(draft.map);
    showInfo("Lokalen Entwurf wiederhergestellt", "warning");
    return true;
  }

  async function loadMap() {
    busy = true;
    try {
      const map = await withLoading("load-map", "Lade Karte...", async () =>
        getMapDocument(),
      );
      mapStore.load(normalizeMapDocument(map));
      await refreshStoredMaps(true);
      readDraft();
      if (draftAvailable) {
        showInfo(
          "Roboter-Karte geladen. Lokaler Browser-Entwurf ist separat verfügbar.",
          "warning",
        );
      } else {
        showInfo("Roboter-Karte geladen");
      }
    } catch (err) {
      showInfo(err instanceof Error ? err.message : "Fehler", "error");
    } finally {
      busy = false;
      hydrationDone = true;
    }
  }

  async function refreshStoredMaps(keepSelection = false) {
    const mapsState = await getStoredMaps();
    storedMaps = mapsState.maps;
    activeMapId = mapsState.active_id;
    if (
      keepSelection &&
      selectedMapId &&
      storedMaps.some((m) => m.id === selectedMapId)
    ) {
      return;
    }
    selectedMapId = activeMapId || storedMaps[0]?.id || "";
  }

  async function activateMap() {
    if (!selectedMapId) {
      showInfo("Keine Karte ausgewählt", "warning");
      return;
    }
    if (selectedMapId === activeMapId) {
      showInfo("Karte ist bereits aktiv");
      return;
    }
    busy = true;
    try {
      const result = await activateStoredMap(selectedMapId);
      if (!result.ok) {
        showInfo(result.error ?? "Aktivieren fehlgeschlagen", "error");
        return;
      }
      clearDraft();
      await loadMap();
      await refreshStoredMaps();
      showInfo("Karte aktiviert");
    } catch (err) {
      showInfo(
        err instanceof Error ? err.message : "Aktivieren fehlgeschlagen",
        "error",
      );
    } finally {
      busy = false;
    }
  }

  async function saveAsNewMap() {
    const suggested = `Karte ${storedMaps.length + 1}`;
    const name = prompt("Name der neuen Karte:", suggested)?.trim();
    if (!name) return;
    busy = true;
    try {
      const result = await createStoredMap({
        name,
        activate: true,
        map: mapPayload(),
      });
      if (!result.ok) {
        showInfo(
          result.error ?? "Speichern als neue Karte fehlgeschlagen",
          "error",
        );
        return;
      }
      clearDraft();
      await loadMap();
      await refreshStoredMaps();
      showInfo("Neue Karte gespeichert und aktiviert");
    } catch (err) {
      showInfo(
        err instanceof Error
          ? err.message
          : "Speichern als neue Karte fehlgeschlagen",
        "error",
      );
    } finally {
      busy = false;
    }
  }

  async function removeMap() {
    if (!selectedMapId) {
      showInfo("Keine Karte ausgewählt", "warning");
      return;
    }
    if (selectedMapId === activeMapId) {
      showInfo("Aktive Karte kann nicht gelöscht werden", "warning");
      return;
    }
    const mapName =
      storedMaps.find((m) => m.id === selectedMapId)?.name ?? selectedMapId;
    if (!confirm(`Karte "${mapName}" wirklich löschen?`)) return;
    busy = true;
    try {
      const result = await deleteStoredMap(selectedMapId);
      if (!result.ok) {
        showInfo(result.error ?? "Löschen fehlgeschlagen", "error");
        return;
      }
      await refreshStoredMaps();
      showInfo("Karte gelöscht");
    } catch (err) {
      showInfo(
        err instanceof Error ? err.message : "Löschen fehlgeschlagen",
        "error",
      );
    } finally {
      busy = false;
    }
  }

  async function saveMap() {
    if (!canSaveMap) {
      showInfo(saveHint, "warning");
      return;
    }
    busy = true;
    try {
      const result = await withLoading(
        "save-map",
        "Speichert Karte...",
        async () => saveMapDocument(mapPayload()),
      );
      if (!result.ok) {
        throw new Error(result.error ?? "Fehler");
      }
      mapStore.markSaved();
      clearDraft();
      toast.success("✓ Karte gespeichert");
    } catch (err) {
      const message = err instanceof Error ? err.message : "Fehler";
      toast.error(`Fehler: ${message}`, 6000, {
        label: "Erneut versuchen",
        handler: saveMap,
      });
      info = message;
      infoTone = "error";
    } finally {
      busy = false;
    }
  }

  async function exportMap() {
    busy = true;
    try {
      const blob = await withLoading(
        "export-map",
        "Exportiere GeoJSON...",
        async () => exportMapGeoJson(),
      );
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = "sunray-map.geojson";
      a.click();
      URL.revokeObjectURL(url);
      showInfo("GeoJSON exportiert");
    } catch (err) {
      showInfo(
        err instanceof Error ? err.message : "Export fehlgeschlagen",
        "error",
      );
    } finally {
      busy = false;
    }
  }

  function importMap() {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".geojson,.json,application/geo+json,application/json";
    input.onchange = async () => {
      const file = input.files?.[0];
      if (!file) return;
      busy = true;
      try {
        const text = await file.text();
        const result = await withLoading(
          "import-map",
          "Importiere GeoJSON...",
          async () => importMapGeoJson(text),
        );
        if (!result.ok) {
          showInfo(result.error ?? "Import fehlgeschlagen", "error");
          return;
        }
        clearDraft();
        await loadMap();
        showInfo("GeoJSON importiert");
      } catch (err) {
        showInfo(
          err instanceof Error ? err.message : "Import fehlgeschlagen",
          "error",
        );
      } finally {
        busy = false;
      }
    };
    input.click();
  }

  $: activeTool = $mapStore.selectedTool;
  $: draftSavedAtLabel = draftSavedAt
    ? new Date(draftSavedAt).toLocaleString("de-DE", {
        dateStyle: "short",
        timeStyle: "short",
      })
    : "";
  $: dockActive = activeTool === "dock";
  $: zoneActive = activeTool === "zone";
  $: moveActive = activeTool === "move";
  $: layer = (
    activeTool === "nogo"
      ? "nogo"
      : activeTool === "zone"
        ? "zone"
        : "perimeter"
  ) as Extract<MapTool, "perimeter" | "nogo" | "zone">;

  function setLayer(l: "perimeter" | "nogo" | "zone") {
    if (l === "nogo" && $mapStore.map.exclusions.length === 0) {
      mapStore.createExclusion();
      return;
    }
    if (l === "zone" && $mapStore.map.zones.length === 0) {
      mapStore.createZone();
      return;
    }
    mapStore.setTool(l);
  }

  function toggleDock() {
    mapStore.setTool(dockActive ? layer : "dock");
  }
  function toggleMove() {
    mapStore.setTool(moveActive ? layer : "move");
  }

  function addNew() {
    if (activeTool === "nogo") mapStore.createExclusion();
  }

  function deleteActive() {
    if (layer === "nogo") {
      mapStore.deleteSelectedExclusion();
      return;
    }
    mapStore.clearActive();
  }

  function removeLastPoint() {
    if (activeTool === "move") {
      if (mapCanvas?.deleteSelectedPoint()) {
        showInfo("Punkt gelöscht", "success");
        return;
      }
      mapStore.setTool(layer);
    }

    if (layer === "nogo" && $mapStore.selectedExclusionIndex !== null) {
      const exclusion =
        $mapStore.map.exclusions[$mapStore.selectedExclusionIndex] ?? [];
      if (exclusion.length === 0) {
        mapStore.deleteSelectedExclusion();
        showInfo("Leere NoGo-Zone gelöscht", "success");
        return;
      }
    }

    mapStore.removeLastPoint();
  }

  function addCurrentRobotPoint() {
    if (moveActive) {
      showInfo(
        "Im Verschieben-Modus können keine neuen Punkte gesetzt werden",
        "warning",
      );
      return;
    }

    if (!allowPointAdd) {
      showInfo(pointAddBlockedReason, "warning");
      return;
    }

    const point = {
      x: Number($telemetry.x.toFixed(2)),
      y: Number($telemetry.y.toFixed(2)),
    };

    const result = mapStore.addPoint(point);
    if (!result.accepted) {
      showInfo(
        result.reason || "Punkt konnte nicht gesetzt werden",
        result.reason === "Punkt liegt bereits an dieser Stelle"
          ? "warning"
          : "error",
      );
      return;
    }

    if (dockActive) {
      showInfo("Docking-Pfadpunkt von aktueller Roboterposition gespeichert");
      return;
    }

    if (activeTool === "nogo") {
      showInfo("NoGo-Punkt von aktueller Roboterposition gespeichert");
      return;
    }

    showInfo("Perimeterpunkt von aktueller Roboterposition gespeichert");
  }

  function newMap() {
    mapStore.reset();
    mapStore.setTool("perimeter");
    hydrationDone = true;
    saveDraft();
    showInfo("Neue Karte gestartet");
  }

  function activateTool(tool: MapTool) {
    if (tool === "nogo" && $mapStore.map.exclusions.length === 0) {
      mapStore.createExclusion();
      return;
    }
    if (tool === "zone" && $mapStore.map.zones.length === 0) {
      mapStore.createZone();
      return;
    }
    mapStore.setTool(tool);
  }

  function createZoneTool() {
    mapStore.createZone();
    showInfo(`Zone ${$mapStore.map.zones.length} gestartet`);
  }

  function selectZone(zoneId: string) {
    mapStore.selectZone(zoneId);
    mapStore.setTool("zone");
  }

  function deleteActiveZone() {
    if (!$mapStore.selectedZoneId) {
      showInfo("Bitte zuerst eine Zone auswählen", "warning");
      return;
    }
    const zoneLabel =
      $mapStore.map.zones.find((zone) => zone.id === $mapStore.selectedZoneId)
        ?.name ?? "Zone";
    mapStore.deleteSelectedZone();
    showInfo(`${zoneLabel} gelöscht`);
  }

  function finishNoGoStep() {
    mapStore.setTool("dock");
    showInfo("NoGo abgeschlossen, jetzt Docking-Pfad aufnehmen");
  }

  function finishPerimeterStep() {
    if ($mapStore.map.perimeter.length < 3) {
      showInfo("Perimeter braucht mindestens 3 Punkte", "warning");
      return;
    }
    if (hasSelfIntersection($mapStore.map.perimeter)) {
      showInfo("Perimeter darf sich nicht selbst schneiden", "error");
      return;
    }
    mapStore.setTool("nogo");
    showInfo(
      "Perimeter abgeschlossen. Optional NoGo markieren oder direkt mit Docking-Pfad weitermachen",
    );
  }

  function finishActiveNoGo() {
    mapStore.selectExclusion(null);
    mapStore.setTool("perimeter");
    showInfo("NoGo-Bereich abgeschlossen");
  }

  function startNewNoGo() {
    mapStore.createExclusion();
    showInfo(`NoGo ${$mapStore.map.exclusions.length} gestartet`);
  }

  function editActiveNoGo() {
    if ($mapStore.selectedExclusionIndex === null) {
      showInfo("Bitte zuerst einen NoGo-Bereich auswählen", "warning");
      return;
    }
    mapStore.setTool("nogo");
    showInfo(`${activeNoGoLabel} aktiv`);
  }

  function deleteActiveNoGo() {
    if ($mapStore.selectedExclusionIndex === null) {
      showInfo("Bitte zuerst einen NoGo-Bereich auswählen", "warning");
      return;
    }
    const label = activeNoGoLabel;
    mapStore.deleteSelectedExclusion();
    showInfo(`${label} gelöscht`);
  }

  function removeLastPerimeterPoint() {
    mapStore.setTool("perimeter");
    mapStore.removeLastPoint();
  }

  function clearDockPath() {
    mapStore.setTool("dock");
    mapStore.clearActive();
  }

  function finishDockPath() {
    if ($mapStore.map.dock.length < 2) {
      showInfo("Docking-Pfad braucht mindestens 2 Punkte", "warning");
      return;
    }
    mapStore.setTool("move");
    showInfo(
      "Docking-Pfad abgeschlossen. Jetzt Validierung und Speichern prüfen",
    );
  }

  function handlePointRejected(
    event: CustomEvent<{ tool: string; reason?: string }>,
  ) {
    showInfo(
      event.detail.reason || "Punkt wuerde Polygon schneiden",
      event.detail.reason ? "warning" : "error",
    );
  }

  $: addBtnTitle = moveActive
    ? "Im Verschieben-Modus nicht verfügbar"
    : dockActive
      ? "Aktuelle Roboterposition als Docking-Pfadpunkt speichern"
      : activeTool === "nogo"
        ? "Aktuelle Roboterposition als NoGo-Punkt speichern"
        : zoneActive
          ? "Aktuelle Roboterposition als Zonenpunkt speichern"
          : "Aktuelle Roboterposition als Perimeterpunkt speichern";

  $: connectionFresh =
    $connection.connected &&
    nowMs - $connection.lastSeen <= CONNECTION_FRESH_MS;
  $: rtkFix = $telemetry.gps_sol === 4;
  $: rtkFloat = $telemetry.gps_sol === 5;
  $: acceptableAccuracy =
    $telemetry.gps_acc > 0 && $telemetry.gps_acc <= GOOD_ACCURACY_M;
  $: mappingSignalReady =
    connectionFresh && (rtkFix || (rtkFloat && acceptableAccuracy));
  $: preflightHint = !$connection.connected
    ? "WLAN getrennt: Aufnahme pausiert"
    : !connectionFresh
      ? "Telemetrie veraltet: kurz auf frische Daten warten"
      : rtkFix
        ? "RTK Fix stabil"
        : rtkFloat && acceptableAccuracy
          ? "RTK Float mit guter Genauigkeit"
          : "Kein stabiles RTK für neue Punkte";
  $: effectivePreflightHint = $mappingTestMode
    ? "Testmodus aktiv: Punktaufnahme und Freigabe sind ohne RTK für UI-Tests erlaubt"
    : preflightHint;

  $: hasNogo = $mapStore.map.exclusions.length > 0;
  $: perimeterTooSmall =
    $mapStore.map.perimeter.length > 0 && $mapStore.map.perimeter.length < 3;
  $: perimeterSelfIntersecting = hasSelfIntersection($mapStore.map.perimeter);
  $: perimeterValid =
    $mapStore.map.perimeter.length >= 3 && !perimeterSelfIntersecting;
  $: dockTooShort =
    $mapStore.map.dock.length > 0 && $mapStore.map.dock.length < 2;
  $: dockEntryPoint =
    $mapStore.map.dock.length > 0 ? $mapStore.map.dock[0] : null;
  $: dockTerminalPoint =
    $mapStore.map.dock.length > 0
      ? $mapStore.map.dock[$mapStore.map.dock.length - 1]
      : null;
  $: dockEntryDistance =
    perimeterValid && dockEntryPoint
      ? minDistanceToPolygon(dockEntryPoint, $mapStore.map.perimeter)
      : Number.POSITIVE_INFINITY;
  $: dockEntryPlausible =
    perimeterValid &&
    dockEntryPoint &&
    (pointInPolygon(dockEntryPoint, $mapStore.map.perimeter) ||
      dockEntryDistance <= MAX_DOCK_ENTRY_DISTANCE_M);
  $: dockPathValid = $mapStore.map.dock.length >= 2 && dockEntryPlausible;
  $: validationIssues = [
    !perimeterValid
      ? perimeterTooSmall
        ? "Perimeter braucht mindestens 3 Punkte"
        : perimeterSelfIntersecting
          ? "Perimeter darf sich nicht selbst schneiden"
          : "Perimeter fehlt noch"
      : "",
    !dockPathValid
      ? dockTooShort
        ? "Docking-Pfad braucht mindestens 2 Punkte"
        : $mapStore.map.dock.length === 0
          ? "Docking-Pfad fehlt noch"
          : "Docking-Pfad muss am Perimeter beginnen oder in seiner Naehe"
      : "",
    !mappingSignalReady && !$mappingTestMode
      ? "Für Aufnahme und Freigabe wird frische RTK-Telemetrie benoetigt"
      : "",
  ].filter(Boolean);
  $: saveHint = validationIssues[0] ?? "";
  $: canSaveMap = !busy && !saveHint;
  $: allowPointAdd = mappingSignalReady || $mappingTestMode;
  $: pointAddBlockedReason = effectivePreflightHint;
  $: noGoPointCount = $mapStore.map.exclusions.reduce(
    (total, exclusion) => total + exclusion.length,
    0,
  );
  $: activeNoGoLabel =
    $mapStore.selectedExclusionIndex !== null
      ? `NoGo ${$mapStore.selectedExclusionIndex + 1}`
      : "Keine aktive NoGo";
  $: workflowSteps = [
    {
      id: "new",
      label: "Neue Karte",
      status:
        $mapStore.map.perimeter.length === 0 &&
        $mapStore.map.dock.length === 0 &&
        noGoPointCount === 0
          ? "active"
          : "done",
      detail:
        $mapStore.map.perimeter.length === 0 &&
        $mapStore.map.dock.length === 0 &&
        noGoPointCount === 0
          ? "Frischen Entwurf starten oder bestehenden Entwurf laden"
          : "Entwurf aktiv",
    },
    {
      id: "rtk",
      label: "RTK prüfen",
      status: mappingSignalReady ? "done" : "active",
      detail: effectivePreflightHint,
    },
    {
      id: "perimeter",
      label: "Grenze aufnehmen",
      status: perimeterValid
        ? "done"
        : mappingSignalReady
          ? "active"
          : "blocked",
      detail: perimeterValid
        ? `${$mapStore.map.perimeter.length} Punkte aufgenommen`
        : `${$mapStore.map.perimeter.length} Punkte aufgenommen`,
    },
    {
      id: "nogo",
      label: "NoGo optional",
      status:
        perimeterValid && hasNogo
          ? "done"
          : perimeterValid
            ? "active"
            : "pending",
      detail: hasNogo
        ? `${$mapStore.map.exclusions.length} NoGo-Bereiche hinterlegt`
        : "Optional: Beete, Inseln oder Sperrbereiche markieren",
    },
    {
      id: "dock",
      label: "Docking-Pfad",
      status: dockPathValid
        ? "done"
        : perimeterValid && mappingSignalReady
          ? "active"
          : perimeterValid
            ? "blocked"
            : "pending",
      detail: dockPathValid
        ? `${$mapStore.map.dock.length} Punkte im Docking-Pfad`
        : `${$mapStore.map.dock.length} Punkte gesetzt`,
    },
    {
      id: "validate",
      label: "Validieren",
      status:
        validationIssues.length === 0
          ? "done"
          : perimeterValid || $mapStore.map.dock.length > 0
            ? "active"
            : "pending",
      detail:
        validationIssues.length === 0
          ? "Karte ist freigabebereit"
          : validationIssues[0],
    },
    {
      id: "save",
      label: "Speichern",
      status:
        !$mapStore.dirty && perimeterValid && dockPathValid
          ? "done"
          : validationIssues.length === 0
            ? "active"
            : "pending",
      detail:
        !$mapStore.dirty && perimeterValid && dockPathValid
          ? "Aktive Karte synchron"
          : "Freigabe speichert die aktive Karte",
    },
  ] satisfies WorkflowStep[];
  $: currentStep =
    workflowSteps.find((step) => step.status === "active") ??
    workflowSteps[workflowSteps.length - 1];
  $: nextStep =
    workflowSteps.find(
      (step) => step.status === "pending" || step.status === "blocked",
    ) ?? null;
  $: primaryBlocker =
    validationIssues[0] ??
    (!mappingSignalReady && !$mappingTestMode ? preflightHint : "");
  $: connectionAgeSeconds =
    $connection.lastSeen > 0
      ? Math.max(0, Math.round((nowMs - $connection.lastSeen) / 1000))
      : null;
  onMount(() => {
    mapHistory = createUndoRedo(structuredClone($mapStore.map), 30);
    window.addEventListener("keydown", handleKeydown);
    const interval = setInterval(() => {
      nowMs = Date.now();
    }, 1000);
    void (async () => {
      await refreshStoredMaps();
      await loadMap();
    })();
    return () => {
      window.removeEventListener("keydown", handleKeydown);
      clearInterval(interval);
      if (infoTimer) clearTimeout(infoTimer);
    };
  });

  $: if (hydrationDone) {
    saveDraft();
  }

  $: if (hydrationDone && mapHistory && !applyingHistory) {
    const snapshot = JSON.stringify($mapStore.map);
    if (snapshot !== lastHistorySnapshot) {
      lastHistorySnapshot = snapshot;
      mapHistory.push(structuredClone($mapStore.map), "Map updated");
    }
  }

  $: if (mapHistory) {
    const history = mapHistory.getHistory();
    undoLabel = history.past[history.past.length - 1]?.description ?? "";
    redoLabel = history.future[0]?.description ?? "";
  }
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="map-stage">
    <!-- Top toolbar -->
    <div class="map-toolbar">
      <span class="mt-section-label">Werkzeuge</span>
      <div class="mt-tools">
        <button
          type="button"
          class:active={moveActive}
          title="Punkte verschieben"
          on:click={toggleMove}>↖ Zeiger</button
        >
        <button
          type="button"
          class:active={activeTool === "perimeter" && !moveActive}
          on:click={() => setLayer("perimeter")}
          ><span class="mt-dot perimeter"></span>Perimeter</button
        >
        <button
          type="button"
          class:active={layer === "nogo" && !moveActive}
          on:click={() => setLayer("nogo")}
        >
          <span class="mt-dot nogo"></span>No-Go
          {#if hasNogo}<span class="mt-badge"
              >{$mapStore.map.exclusions.length}</span
            >{/if}
        </button>
        <button type="button" class:active={dockActive} on:click={toggleDock}
          ><span class="mt-dot dock"></span>Dock-Pfad</button
        >
        <button
          type="button"
          class:active={zoneActive}
          on:click={() => setLayer("zone")}
          ><span class="mt-dot zone"></span>Zone</button
        >
      </div>

      <div class="mt-sep"></div>

      <div class="mt-actions">
        <button
          type="button"
          title={undoLabel
            ? `Rückgängig: ${undoLabel}`
            : "Rückgängig (Strg/Cmd+Z)"}
          disabled={!mapHistory || !mapHistory.canUndo()}
          on:click={undoMapEdit}
          >↶ Undo{#if undoLabel}<span class="mt-sub-label">{undoLabel}</span
            >{/if}</button
        >
        <button
          type="button"
          title={redoLabel
            ? `Wiederholen: ${redoLabel}`
            : "Wiederholen (Strg/Cmd+Shift+Z)"}
          disabled={!mapHistory || !mapHistory.canRedo()}
          on:click={redoMapEdit}
          >↷ Redo{#if redoLabel}<span class="mt-sub-label">{redoLabel}</span
            >{/if}</button
        >
        <select
          class="mt-map-select"
          bind:value={selectedMapId}
          title="Gespeicherte Karten"
          disabled={busy || storedMaps.length === 0}
        >
          {#each storedMaps as map}
            <option value={map.id}
              >{map.name}{map.id === activeMapId ? " (aktiv)" : ""}</option
            >
          {/each}
        </select>
        <button
          type="button"
          title="Ausgewählte Karte aktivieren"
          disabled={busy}
          on:click={activateMap}>Aktivieren</button
        >
        <button
          type="button"
          title="Aktuelle Bearbeitung als neue Karte speichern"
          disabled={busy}
          on:click={saveAsNewMap}>Speichern als</button
        >
        <button
          type="button"
          title="Ausgewählte Karte löschen"
          disabled={busy || selectedMapId === activeMapId}
          on:click={removeMap}>Löschen</button
        >
        <button type="button" title="GeoJSON exportieren" on:click={exportMap}
          >↑ GeoJSON</button
        >
        <button type="button" title="GeoJSON importieren" on:click={importMap}
          >↓ GeoJSON</button
        >
        <button type="button" disabled={busy} on:click={loadMap}>Laden</button>
        <button
          type="button"
          class:unsaved={$mapStore.dirty}
          disabled={!$mapStore.dirty || !canSaveMap}
          on:click={saveMap}
          >Speichern{#if $mapStore.dirty}*{/if}</button
        >
      </div>
    </div>

    {#if info}
      <div class={`map-toast ${infoTone}`} role="status" aria-live="polite">
        {info}
      </div>
    {/if}

    <MapCanvas
      bind:this={mapCanvas}
      showHeader={false}
      showViewportActions={false}
      interactive={true}
      showRuntimeLayers={false}
      {allowPointAdd}
      {pointAddBlockedReason}
      on:pointrejected={handlePointRejected}
    />

    <!-- Floating action panel -->
    <div class="toolbar-wrap">
      {#if layer === "nogo"}
        <div class="item-bar">
          {#each $mapStore.map.exclusions as _, i}
            <button
              type="button"
              class="item-btn nogo"
              class:active={$mapStore.selectedExclusionIndex === i}
              on:click={() => {
                mapStore.selectExclusion(i);
                mapStore.setTool("nogo");
              }}>NoGo {i + 1}</button
            >
          {/each}
          <button type="button" class="item-new" on:click={addNew}>+ Neu</button
          >
        </div>
      {/if}

      <div class="big-btns">
        <button
          type="button"
          class="big add"
          disabled={moveActive}
          title={addBtnTitle}
          on:click={addCurrentRobotPoint}>+</button
        >
        <button
          type="button"
          class="big remove"
          title="Letzten Punkt entfernen"
          on:click={removeLastPoint}>−</button
        >
      </div>
    </div>

    <!-- Bottom left: zoom -->
    <div class="zoom-stack">
      <button
        type="button"
        title="Heranzoomen"
        on:click={() => mapCanvas?.zoomIn()}>+</button
      >
      <button
        type="button"
        title="Herauszoomen"
        on:click={() => mapCanvas?.zoomOut()}>−</button
      >
      <button
        type="button"
        title="Auf Inhalt zoomen"
        on:click={() => mapCanvas?.fitToContent()}>◎</button
      >
    </div>
  </div>

  <svelte:fragment slot="bottom">
    <BottomPanel />
  </svelte:fragment>

  <svelte:fragment slot="sidebar">
    <div class="sidebar-inner">
      <div class="sb-section">
        <span class="sb-label">Mapping-Assistent</span>
        <div class="assistant-card">
          <strong>{currentStep.label}</strong>
          <span>{currentStep.detail}</span>
          {#if nextStep}
            <span>Als nächstes: {nextStep.label}</span>
          {/if}
          {#if primaryBlocker}
            <span class="warn-copy">Blocker: {primaryBlocker}</span>
          {/if}
        </div>
        <div class="sb-actions">
          <button
            type="button"
            class="sb-btn"
            title="Neue Karte"
            disabled={busy}
            on:click={newMap}>Neu</button
          >
          <button
            type="button"
            class="sb-btn"
            title={undoLabel ? `Rückgängig: ${undoLabel}` : "Rückgängig"}
            disabled={!mapHistory || !mapHistory.canUndo()}
            on:click={undoMapEdit}>Undo</button
          >
          <button
            type="button"
            class="sb-btn"
            title={redoLabel ? `Wiederholen: ${redoLabel}` : "Wiederholen"}
            disabled={!mapHistory || !mapHistory.canRedo()}
            on:click={redoMapEdit}>Redo</button
          >
          <button
            type="button"
            class="sb-btn"
            title="Neu laden"
            disabled={busy}
            on:click={loadMap}>Laden</button
          >
        </div>
      </div>

      <div class="sb-section">
        <span class="sb-label">2. Grenze aufnehmen</span>
        <div class="assistant-card" class:active={activeTool === "perimeter"}>
          <strong>{$mapStore.map.perimeter.length} Punkte</strong>
          <span
            >Roboter entlang der Grenze abstellen und Punkt bei gutem RTK
            setzen.</span
          >
          <div class="assistant-actions">
            <button
              type="button"
              class="sb-btn"
              on:click={() => activateTool("perimeter")}>Perimeter</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={$mapStore.map.perimeter.length < 3}
              on:click={finishPerimeterStep}>Abschliessen</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={!perimeterValid}
              on:click={finishNoGoStep}>Weiter</button
            >
          </div>
        </div>
      </div>

      <div class="sb-section">
        <span class="sb-label">3. NoGo optional</span>
        <div class="assistant-card" class:active={activeTool === "nogo"}>
          <strong>{$mapStore.map.exclusions.length} Bereiche</strong>
          <span>Optional für Beete, Inseln oder andere Sperrflächen.</span>
          <span>{activeNoGoLabel}</span>
          <div class="assistant-actions">
            <button
              type="button"
              class="sb-btn"
              disabled={!perimeterValid}
              on:click={() => activateTool("nogo")}>NoGo</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={!perimeterValid}
              on:click={startNewNoGo}>+ Neu</button
            >
          </div>
          <div class="assistant-actions">
            <button
              type="button"
              class="sb-btn"
              disabled={$mapStore.selectedExclusionIndex === null}
              on:click={editActiveNoGo}>Bearbeiten</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={$mapStore.selectedExclusionIndex === null}
              on:click={finishActiveNoGo}>Abschliessen</button
            >
          </div>
        </div>
      </div>

      {#if SHOW_ADVANCED_MAP_TUNING}
        <div class="sb-section">
          <span class="sb-label">NoGo-Regeln</span>
          <div class="assistant-card">
            <strong>Interne Planner-Regeln</strong>
            <span
              >NoGo-Typen und Clearance werden im Standardmodus intern über
              Defaults und Diagnose-/Servicewerte gesteuert.</span
            >
          </div>
        </div>
      {/if}

      <div class="sb-section">
        <span class="sb-label">4. Docking-Pfad</span>
        <div
          class="assistant-card"
          class:active={dockActive}
          class:blocked={!perimeterValid}
        >
          <strong>{$mapStore.map.dock.length} Punkte</strong>
          <span>Pfad vom Garten zur Ladestation vollstaendig aufnehmen.</span>
          <div class="assistant-actions">
            <button
              type="button"
              class="sb-btn"
              disabled={!perimeterValid}
              on:click={() => activateTool("dock")}>Docking-Pfad</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={$mapStore.map.dock.length === 0}
              on:click={clearDockPath}>Leeren</button
            >
          </div>
          <div class="assistant-actions">
            <button
              type="button"
              class="sb-btn"
              disabled={$mapStore.map.dock.length < 2}
              on:click={finishDockPath}>Abschliessen</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={!dockPathValid}
              on:click={() => mapStore.setTool("move")}>Zur Kontrolle</button
            >
          </div>
        </div>
      </div>

      {#if SHOW_ADVANCED_MAP_TUNING}
        <div class="sb-section">
          <span class="sb-label">Docking-Logik</span>
          <div class="assistant-card">
            <strong>Service-Tuning</strong>
            <span
              >Docking-Tuning und lokale Planner-Parameter sind im Standardmodus
              ausgeblendet und werden über interne Defaults beziehungsweise
              Config geregelt.</span
            >
          </div>
        </div>
      {/if}

      <div class="sb-section">
        <span class="sb-label">5. Validieren & Speichern</span>
        <div class="assistant-card" class:blocked={validationIssues.length > 0}>
          <strong
            >{validationIssues.length === 0
              ? "Karte freigabebereit"
              : "Noch offen"}</strong
          >
          {#if validationIssues.length > 0}
            <ul class="issue-list">
              {#each validationIssues as issue}
                <li>{issue}</li>
              {/each}
            </ul>
          {:else}
            <span>Perimeter, Docking-Pfad und RTK-Prüfung sind plausibel.</span>
          {/if}
          <div class="assistant-actions assistant-actions--stacked">
            <button
              type="button"
              class="sb-btn primary"
              class:unsaved={$mapStore.dirty}
              disabled={!$mapStore.dirty || !canSaveMap}
              on:click={saveMap}>Karte freigeben</button
            >
            <div class="sb-actions">
              <button type="button" class="sb-btn" on:click={importMap}
                >Import</button
              >
              <button type="button" class="sb-btn" on:click={exportMap}
                >Export</button
              >
            </div>
          </div>
        </div>
      </div>

      <div class="sb-section">
        <span class="sb-label">Entwurf</span>
        <div class="sb-stat">
          <span>Status</span><strong
            >{$mapStore.dirty ? "Ungespeichert" : "Synchron"}</strong
          >
        </div>
        <div class="sb-stat">
          <span>Perimeter</span><strong
            >{$mapStore.map.perimeter.length} Punkte</strong
          >
        </div>
        <div class="sb-stat">
          <span>Docking-Pfad</span><strong
            >{$mapStore.map.dock.length} Punkte</strong
          >
        </div>
        <div class="sb-stat">
          <span>NoGo</span><strong>{$mapStore.map.exclusions.length}</strong>
        </div>
        {#if draftAvailable}
          <div class="assistant-card">
            <strong>Lokaler Browser-Entwurf vorhanden</strong>
            <span>
              Separat vom Roboterstand gespeichert{#if draftSavedAtLabel}
                : {draftSavedAtLabel}{/if}
            </span>
            <div class="assistant-actions">
              <button type="button" class="sb-btn" on:click={restoreDraft}
                >Entwurf laden</button
              >
              <button type="button" class="sb-btn" on:click={clearDraft}
                >Entwurf verwerfen</button
              >
            </div>
          </div>
        {/if}
      </div>

      <div class="sb-section">
        <span class="sb-label">Zonen</span>
        <div class="assistant-card" class:active={zoneActive}>
          <strong>{$mapStore.map.zones.length} Zonen</strong>
          <span
            >Zone auf der Karte anlegen, dann Punkte setzen und per Klick
            zwischen Zonen wechseln.</span
          >
          <div class="assistant-actions">
            <button type="button" class="sb-btn" on:click={createZoneTool}
              >+ Neue Zone</button
            >
            <button
              type="button"
              class="sb-btn"
              disabled={!$mapStore.selectedZoneId}
              on:click={deleteActiveZone}>Löschen</button
            >
          </div>
        </div>
        <div class="zone-list">
          {#each $mapStore.map.zones as zone}
            <button
              type="button"
              class="zone-chip"
              class:active={$mapStore.selectedZoneId === zone.id}
              on:click={() => selectZone(zone.id)}
            >
              <span class="zone-chip-dot"></span>
              <span class="zone-chip-name">{zone.name}</span>
              <span class="zone-chip-count">{zone.polygon.length}</span>
            </button>
          {/each}
          {#if $mapStore.map.zones.length === 0}
            <span class="zone-empty">Noch keine Zonen angelegt.</span>
          {/if}
        </div>
      </div>
    </div></svelte:fragment
  >
</PageLayout>

<style>
  .map-stage {
    position: relative;
    height: 100%;
    background: #070d18;
  }

  /* ── Top toolbar ── */
  .map-toolbar {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    z-index: 20;
    display: flex;
    align-items: center;
    gap: 10px;
    background: #0f1829;
    border-bottom: 1px solid #1e3a5f;
    padding: 6px 16px;
    flex-shrink: 0;
  }

  .mt-section-label {
    font-size: 0.65rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: #64748b;
    white-space: nowrap;
    padding-right: 4px;
    border-right: 1px solid #1e3a5f;
    margin-right: 2px;
  }

  .mt-tools {
    display: flex;
    align-items: center;
    gap: 0.15rem;
  }

  .mt-tools button,
  .mt-actions button {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    padding: 0.25rem 0.6rem;
    border-radius: 0.35rem;
    border: 1px solid transparent;
    background: transparent;
    color: #64748b;
    font-size: 0.72rem;
    font-weight: 600;
    cursor: pointer;
    white-space: nowrap;
    transition:
      color 0.15s,
      background 0.15s;
  }

  .mt-sub-label {
    display: block;
    margin-top: 0.12rem;
    font-size: 0.64rem;
    line-height: 1.1;
    color: #7a8da8;
    max-width: 8rem;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .mt-tools button:hover,
  .mt-actions button:hover:not(:disabled) {
    background: rgba(30, 58, 95, 0.4);
    color: #e2e8f0;
  }

  .mt-tools button.active {
    background: rgba(30, 58, 95, 0.35);
    border-color: #2563eb;
    color: #93c5fd;
  }

  .mt-actions button.unsaved {
    border-color: #d97706;
    color: #fbbf24;
  }

  .mt-actions button:disabled {
    opacity: 0.4;
    cursor: not-allowed;
  }

  .mt-dot {
    width: 0.4rem;
    height: 0.4rem;
    border-radius: 50%;
    flex-shrink: 0;
  }
  .mt-dot.perimeter {
    background: #2563eb;
  }
  .mt-dot.nogo {
    background: #dc2626;
  }
  .mt-dot.dock {
    background: #d97706;
  }
  .mt-dot.dock-corridor {
    background: #facc15;
  }
  .mt-dot.zone {
    background: #67e8f9;
  }

  .mt-badge {
    background: #1e3a5f;
    color: #60a5fa;
    border-radius: 8px;
    padding: 0px 4px;
    font-size: 0.62rem;
    font-family: monospace;
  }

  .mt-sep {
    flex: 1;
  }

  .mt-actions {
    display: flex;
    align-items: center;
    gap: 0.15rem;
  }

  .mt-map-select {
    min-width: 10rem;
    max-width: 15rem;
    padding: 0.24rem 0.5rem;
    border-radius: 0.35rem;
    border: 1px solid #1e3a5f;
    background: #0b1424;
    color: #cbd5e1;
    font-size: 0.72rem;
    font-weight: 600;
  }

  .mt-map-select:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .map-toast {
    position: absolute;
    top: 3.2rem;
    left: 50%;
    transform: translateX(-50%);
    z-index: 30;
    max-width: min(32rem, calc(100% - 2rem));
    padding: 0.65rem 0.9rem;
    border-radius: 0.7rem;
    border: 1px solid rgba(37, 99, 235, 0.5);
    background: rgba(7, 13, 24, 0.9);
    color: #dbeafe;
    font-size: 0.72rem;
    font-weight: 600;
    text-align: center;
    box-shadow: 0 12px 28px rgba(0, 0, 0, 0.35);
    backdrop-filter: blur(8px);
    pointer-events: none;
  }

  .map-toast.success {
    border-color: rgba(22, 101, 52, 0.8);
    background: rgba(10, 30, 15, 0.92);
    color: #86efac;
  }

  .map-toast.warning {
    border-color: rgba(217, 119, 6, 0.82);
    background: rgba(40, 24, 0, 0.92);
    color: #fbbf24;
  }

  .map-toast.error {
    border-color: rgba(220, 38, 38, 0.82);
    background: rgba(36, 10, 10, 0.92);
    color: #fca5a5;
  }

  /* ── Bottom center toolbar ── */
  .toolbar-wrap {
    position: absolute;
    bottom: 1rem;
    left: 50%;
    transform: translateX(-50%);
    z-index: 10;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.4rem;
    min-width: 320px;
  }

  .layer-bar {
    display: flex;
    align-items: center;
    gap: 0.2rem;
    padding: 0.28rem 0.45rem;
    background: rgba(10, 16, 32, 0.94);
    border: 1px solid #1e3a5f;
    border-radius: 0.6rem;
    backdrop-filter: blur(6px);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
    min-height: 2rem;
  }

  .layer-dot {
    width: 0.45rem;
    height: 0.45rem;
    border-radius: 50%;
    flex-shrink: 0;
  }
  .layer-dot.perimeter {
    background: #2563eb;
  }
  .layer-dot.nogo {
    background: #dc2626;
  }

  .item-bar {
    display: flex;
    align-items: center;
    gap: 0.25rem;
    padding: 0.22rem 0.4rem;
    background: rgba(10, 16, 32, 0.94);
    border: 1px solid #1e3a5f;
    border-radius: 0.5rem;
    flex-wrap: wrap;
    max-width: 380px;
    min-width: 280px;
    min-height: 1.8rem;
  }

  .item-btn {
    padding: 0.22rem 0.5rem;
    border-radius: 0.35rem;
    border: 1px solid #1a2a40;
    background: #0a1020;
    color: #64748b;
    font-size: 0.62rem;
    font-weight: 600;
    cursor: pointer;
  }

  .item-btn:hover {
    border-color: #0891b2;
    color: #67e8f9;
  }
  .item-btn.active {
    border-color: #0891b2;
    background: #082f49;
    color: #67e8f9;
  }
  .item-btn.nogo:hover {
    border-color: #dc2626;
    color: #fca5a5;
  }
  .item-btn.nogo.active {
    border-color: #dc2626;
    background: #450a0a;
    color: #fca5a5;
  }
  .item-btn.corridor:hover {
    border-color: #facc15;
    color: #fde68a;
  }
  .item-btn.corridor.active {
    border-color: #facc15;
    background: rgba(113, 63, 18, 0.55);
    color: #fde68a;
  }

  .zone-list {
    display: flex;
    flex-wrap: wrap;
    gap: 0.35rem;
    margin-top: 0.5rem;
  }

  .zone-chip {
    display: inline-flex;
    align-items: center;
    gap: 0.35rem;
    padding: 0.25rem 0.55rem;
    border-radius: 999px;
    border: 1px solid #1e3a5f;
    background: #0a1020;
    color: #cbd5e1;
    font-size: 0.66rem;
    cursor: pointer;
  }

  .zone-chip.active {
    border-color: #67e8f9;
    background: rgba(8, 47, 73, 0.72);
    color: #a5f3fc;
  }

  .zone-chip-dot {
    width: 0.35rem;
    height: 0.35rem;
    border-radius: 50%;
    background: #67e8f9;
    flex-shrink: 0;
  }

  .zone-chip-name {
    max-width: 8rem;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .zone-chip-count {
    font-family: monospace;
    font-size: 0.62rem;
    color: #67e8f9;
  }

  .zone-empty {
    font-size: 0.68rem;
    color: #64748b;
  }

  .item-new {
    padding: 0.22rem 0.5rem;
    border-radius: 0.35rem;
    border: 1px solid #1e3a5f;
    background: transparent;
    color: #475569;
    font-size: 0.62rem;
    font-weight: 600;
    cursor: pointer;
  }

  .item-new:hover {
    color: #94a3b8;
    border-color: #334155;
  }
  .item-new.corridor:hover {
    color: #fde68a;
    border-color: #facc15;
  }

  .divider {
    width: 1px;
    height: 1.1rem;
    background: #1e3a5f;
    margin: 0 0.12rem;
    flex-shrink: 0;
  }

  .big-btns {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.4rem;
    width: 100%;
    min-width: 280px;
    min-height: 2.8rem;
  }

  .big {
    height: 2.6rem;
    border-radius: 0.5rem;
    border: 1px solid transparent;
    font-size: 1.4rem;
    font-weight: 700;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .big:disabled {
    opacity: 0.35;
    cursor: default;
  }

  .big.add {
    background: rgba(12, 26, 58, 0.94);
    border-color: #2563eb;
    color: #93c5fd;
  }

  .big.add:hover:not(:disabled) {
    background: rgba(30, 58, 95, 0.94);
  }

  .big.remove {
    background: rgba(69, 10, 10, 0.94);
    border-color: #dc2626;
    color: #fca5a5;
  }

  .big.remove:hover {
    background: rgba(100, 15, 15, 0.94);
  }

  /* ── Bottom left zoom ── */
  .zoom-stack {
    position: absolute;
    bottom: 1rem;
    left: 0.75rem;
    z-index: 15;
    display: flex;
    flex-direction: row;
    gap: 0.35rem;
    min-width: 5.6rem;
    min-height: 2rem;
  }

  .zoom-stack button {
    width: 1.8rem;
    height: 1.8rem;
    border-radius: 0.4rem;
    border: 1px solid #1e3a5f;
    background: rgba(15, 24, 41, 0.94);
    color: #60a5fa;
    font-size: 0.88rem;
    font-weight: 700;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .zoom-stack button:hover {
    background: rgba(30, 58, 95, 0.96);
    color: #93c5fd;
  }

  .sb-section {
    display: grid;
    gap: 0.35rem;
    padding: 0.55rem 0.65rem;
    border-bottom: 1px solid #0f1829;
  }

  .sidebar-inner {
    display: grid;
  }

  .sb-label {
    color: #7a8da8;
    font-size: 0.59rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .sb-actions {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.3rem;
  }

  .assistant-actions {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.3rem;
  }

  .assistant-actions--stacked {
    grid-template-columns: 1fr;
  }

  .sb-btn {
    padding: 0.35rem 0.4rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.4rem;
    background: #0f1829;
    color: #94a3b8;
    font-size: 0.62rem;
    font-weight: 600;
    cursor: pointer;
    text-align: center;
  }

  .sb-btn:hover:not(:disabled) {
    background: #0c1a3a;
    color: #e2e8f0;
  }
  .sb-btn:disabled {
    opacity: 0.4;
    cursor: not-allowed;
  }

  .sb-btn.primary {
    border-color: #2563eb;
    background: #0c1a3a;
    color: #93c5fd;
  }

  .sb-btn.unsaved {
    border-color: #d97706;
    background: rgba(28, 18, 0, 0.6);
    color: #fbbf24;
  }

  .assistant-card {
    display: grid;
    gap: 0.35rem;
    padding: 0.65rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: rgba(15, 24, 41, 0.76);
  }

  .assistant-card strong {
    color: #e2e8f0;
    font-size: 0.74rem;
  }

  .assistant-card span {
    color: #94a3b8;
    font-size: 0.66rem;
    line-height: 1.4;
  }

  .warn-copy {
    color: #fca5a5 !important;
  }

  .assistant-card.active {
    border-color: #2563eb;
    background: rgba(12, 26, 58, 0.8);
  }

  .assistant-card.blocked {
    border-color: #dc2626;
    background: rgba(69, 10, 10, 0.32);
  }

  .telemetry-card {
    gap: 0.55rem;
  }

  .sb-stat {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    font-size: 0.62rem;
    color: #64748b;
  }

  .sb-stat strong {
    color: #94a3b8;
    font-size: 0.64rem;
  }

  .issue-list {
    margin: 0;
    padding-left: 1rem;
    color: #fca5a5;
    font-size: 0.66rem;
    line-height: 1.45;
  }

  @media (max-width: 900px) {
    .telemetry-grid {
      grid-template-columns: 1fr;
    }
  }
</style>
