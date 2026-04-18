<script lang="ts">
  import { onMount } from "svelte";
  import MapCanvas from "../components/Map/MapCanvas.svelte";
  import DashboardSidebar from "../components/Dashboard/DashboardSidebar.svelte";
  import BottomPanel from "../components/Dashboard/BottomPanel.svelte";
  import PageLayout from "../components/PageLayout.svelte";
  import { getMapDocument } from "../api/rest";
  import { mapGpsOrigin } from "../stores/mapGpsOrigin";
  import { mapStore, type Point, type Zone } from "../stores/map";
  import { telemetry } from "../stores/telemetry";
  import { normalizeMapDocumentForUi } from "../utils/mapHelpers";

  let sidebarCollapsed = false;
  let mapCanvas: MapCanvas;

  async function loadOverviewMap() {
    try {
      const map = await getMapDocument();
      const normalized = normalizeMapDocumentForUi(map as any);
      mapGpsOrigin.set(normalized.gpsOrigin);
      mapStore.load(normalized.map as any);
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
