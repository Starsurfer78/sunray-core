<script lang="ts">
  import { onMount } from "svelte";
  import MapCanvas from "../components/Map/MapCanvas.svelte";
  import DashboardSidebar from "../components/Dashboard/DashboardSidebar.svelte";
  import BottomPanel from "../components/Dashboard/BottomPanel.svelte";
  import PageLayout from "../components/PageLayout.svelte";
  import { getMapDocument, type MapZone } from "../api/rest";
  import { mapGpsOrigin } from "../stores/mapGpsOrigin";
  import { mapStore, type Point, type Zone } from "../stores/map";
  import { telemetry } from "../stores/telemetry";
  import { normalizePoints, normalizeZone } from "../utils/mapHelpers";

  let sidebarCollapsed = false;
  let mapCanvas: MapCanvas;

  async function loadOverviewMap() {
    try {
      const map = await getMapDocument();
      const rawPerimeter = normalizePoints(map.perimeter);
      let projectPoints = (pts: Point[]): Point[] => pts;
      mapGpsOrigin.set(null);
      if (rawPerimeter.length >= 3) {
        let sumX = 0;
        let sumY = 0;
        for (const p of rawPerimeter) {
          sumX += p.x;
          sumY += p.y;
        }
        const avgX = sumX / rawPerimeter.length;
        const avgY = sumY / rawPerimeter.length;
        const spanX =
          Math.max(...rawPerimeter.map((p) => p.x)) -
          Math.min(...rawPerimeter.map((p) => p.x));
        const spanY =
          Math.max(...rawPerimeter.map((p) => p.y)) -
          Math.min(...rawPerimeter.map((p) => p.y));
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
          const refLon = avgX;
          const refLat = avgY;
          mapGpsOrigin.set({ lat: refLat, lon: refLon });
          const mercY0 = Math.log(Math.tan(Math.PI / 4 + (refLat * toRad) / 2));
          projectPoints = (pts: Point[]): Point[] =>
            pts.map((p) => ({
              x: (p.x - refLon) * EARTH_R * toRad,
              y:
                (Math.log(Math.tan(Math.PI / 4 + (p.y * toRad) / 2)) - mercY0) *
                EARTH_R,
            }));
        }
      }

      const perimeter = projectPoints(rawPerimeter);
      let dock = projectPoints(normalizePoints(map.dock));
      if (perimeter.length >= 3 && dock.length >= 2) {
        const entry = dock[0];
        const terminal = dock[dock.length - 1];
        const entryOk =
          pointInPolygon(entry, perimeter) ||
          minDistanceToPolygon(entry, perimeter) <= 2.0;
        const terminalOk =
          pointInPolygon(terminal, perimeter) ||
          minDistanceToPolygon(terminal, perimeter) <= 2.0;
        if (!entryOk && terminalOk) {
          dock = [...dock].reverse();
        }
      }
      mapStore.load({
        perimeter,
        dock,
        exclusions: (map.exclusions ?? []).map((exclusion) =>
          projectPoints(normalizePoints(exclusion as Array<[number, number]>)),
        ),
        zones: (map.zones ?? []).map((zone, index) => {
          const z = normalizeZone(zone as MapZone, index);
          return { ...z, polygon: projectPoints(z.polygon) };
        }),
      });
    } catch {
      // map not available
    }
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

  type Segment = { a: Point; b: Point };

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

  onMount(() => {
    void loadOverviewMap();
  });
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="map-stage">
    <div class="map-badge">
      <span class="map-badge-label">Hauptgarten</span>
      <strong>{$telemetry.x.toFixed(2)} / {$telemetry.y.toFixed(2)} m</strong>
    </div>

    <MapCanvas
      bind:this={mapCanvas}
      showHeader={false}
      showViewportActions={false}
      showZoomControls={false}
      interactive={false}
    />

    <!-- Zoom controls -->
    <div class="zoom-controls">
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
    <DashboardSidebar />
  </svelte:fragment>
</PageLayout>

<style>
  .map-stage {
    position: relative;
    height: 100%;
    background: #070d18;
  }

  .map-badge {
    position: absolute;
    top: 0.75rem;
    left: 0.75rem;
    z-index: 2;
    display: grid;
    gap: 0.12rem;
    padding: 0.55rem 0.75rem;
    border-radius: 0.6rem;
    background: rgba(15, 24, 41, 0.92);
    border: 1px solid #1e3a5f;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.28);
  }

  .map-badge-label {
    color: #60a5fa;
    font-size: 0.65rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .map-badge strong {
    color: #dbeafe;
    font-size: 0.75rem;
  }

  .zoom-controls {
    position: absolute;
    bottom: 0.5rem;
    left: 0.75rem;
    z-index: 15;
    display: flex;
    flex-direction: row;
    gap: 0.35rem;
  }

  .zoom-controls button {
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

  .zoom-controls button:hover {
    background: rgba(30, 58, 95, 0.96);
    color: #93c5fd;
  }

  @media (max-width: 900px) {
    .map-stage {
      height: 460px;
    }
  }
</style>
