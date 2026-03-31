<script lang="ts">
  import { onMount } from "svelte";
  import MapCanvas from "../components/Map/MapCanvas.svelte";
  import DashboardSidebar from "../components/Dashboard/DashboardSidebar.svelte";
  import { getMapDocument, type MapZone } from "../api/rest";
  import { mapStore, type Point, type Zone } from "../stores/map";
  import { telemetry } from "../stores/telemetry";

  let mapStatus = "Bereit";
  let sidebarCollapsed = false;

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
        zones: (map.zones ?? []).map((zone, index) => normalizeZone(zone, index)),
      });
      mapStatus = "Synchron";
    } catch {
      mapStatus = "Map nicht verfuegbar";
    }
  }

  onMount(() => {
    void loadOverviewMap();
  });
</script>

<main class="page">
  <section class="layout" class:collapsed={sidebarCollapsed}>
    <div class="main-column">
      <div class="map-stage">
        <div class="map-badge">
          <span class="map-badge-label">Hauptgarten</span>
          <strong
            >{$telemetry.x.toFixed(2)} / {$telemetry.y.toFixed(2)} m</strong
          >
        </div>

        <MapCanvas
          showHeader={false}
          showViewportActions={false}
          showZoomControls={true}
          interactive={false}
          height={680}
        />
      </div>
    </div>

    <DashboardSidebar
      {sidebarCollapsed}
      on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
    />
  </section>
</main>

<style>
  .page {
    height: 100%;
  }

  .layout {
    position: relative;
    height: 100%;
    overflow: hidden;
  }

  .main-column {
    min-width: 0;
    height: 100%;
  }

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

  @media (max-width: 900px) {
    .layout {
      min-height: auto;
    }

    .map-stage {
      height: 460px;
    }
  }
</style>
