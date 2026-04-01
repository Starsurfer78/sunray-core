<script lang="ts">
  import { onMount } from "svelte";
  import MapCanvas from "../components/Map/MapCanvas.svelte";
  import DashboardSidebar from "../components/Dashboard/DashboardSidebar.svelte";
  import BottomPanel from "../components/Dashboard/BottomPanel.svelte";
  import PageLayout from "../components/PageLayout.svelte";
  import { getMapDocument, type MapZone } from "../api/rest";
  import { mapStore, type Point, type Zone } from "../stores/map";
  import { telemetry } from "../stores/telemetry";

  let sidebarCollapsed = false;
  let mapCanvas: MapCanvas;

  function normalizePoints(
    points: Array<[number, number]> | Point[] | undefined,
  ): Point[] {
    if (!points) return [];
    return points.map((point) =>
      Array.isArray(point) ? { x: point[0], y: point[1] } : point,
    );
  }

  function normalizeZone(zone: MapZone, index: number): Zone {
    return {
      id: zone.id,
      order: zone.order ?? index + 1,
      polygon: normalizePoints(zone.polygon),
      settings: {
        name: zone.settings.name ?? `Zone ${index + 1}`,
        stripWidth: zone.settings.stripWidth ?? 0.18,
        angle: zone.settings.angle ?? 0,
        edgeMowing: zone.settings.edgeMowing ?? true,
        edgeRounds: zone.settings.edgeRounds ?? 1,
        speed: zone.settings.speed ?? 1.0,
        pattern: zone.settings.pattern ?? "stripe",
      },
    };
  }

  async function loadOverviewMap() {
    try {
      const map = await getMapDocument();
      mapStore.load({
        perimeter: normalizePoints(map.perimeter),
        dock: normalizePoints(map.dock),
        mow: normalizePoints(map.mow),
        exclusions: (map.exclusions ?? []).map((exclusion) =>
          normalizePoints(exclusion as Array<[number, number]>),
        ),
        zones: (map.zones ?? []).map((zone, index) =>
          normalizeZone(zone, index),
        ),
      });
    } catch {
      // map not available
    }
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
      height={680}
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
