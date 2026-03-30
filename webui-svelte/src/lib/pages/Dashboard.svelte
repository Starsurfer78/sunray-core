<script lang="ts">
  import { onMount } from 'svelte'
  import MapCanvas from '../components/Map/MapCanvas.svelte'
  import DashboardSidebar from '../components/Dashboard/DashboardSidebar.svelte'
  import { getMapDocument } from '../api/rest'
  import { mapStore, type Point, type Zone } from '../stores/map'
  import { telemetry } from '../stores/telemetry'

  let mapStatus = 'Bereit'

  function normalizePoints(points: Array<[number, number]> | Point[] | undefined): Point[] {
    if (!points) return []
    return points.map((point) => Array.isArray(point) ? { x: point[0], y: point[1] } : point)
  }

  async function loadOverviewMap() {
    try {
      const map = await getMapDocument()
      mapStore.load({
        perimeter: normalizePoints(map.perimeter),
        dock: normalizePoints(map.dock),
        mow: normalizePoints(map.mow),
        exclusions: (map.exclusions ?? []).map((exclusion) => normalizePoints(exclusion as Array<[number, number]>)),
        zones: (map.zones ?? []).map((zone: Zone) => ({
          ...zone,
          polygon: normalizePoints(zone.polygon),
        })),
      })
      mapStatus = 'Synchron'
    } catch {
      mapStatus = 'Map nicht verfuegbar'
    }
  }

  onMount(() => {
    void loadOverviewMap()
  })
</script>

<main class="page">
  <section class="layout">
    <div class="main-column">
      <div class="map-stage">
        <div class="map-badge">
          <span class="map-badge-label">Hauptgarten</span>
          <strong>{$telemetry.x.toFixed(2)} / {$telemetry.y.toFixed(2)} m</strong>
        </div>

        <MapCanvas showHeader={false} showViewportActions={false} interactive={false} height={680} />

        <div class="map-controls">
          <button type="button">+</button>
          <button type="button">-</button>
          <button type="button">◎</button>
        </div>
      </div>
    </div>

    <DashboardSidebar />
  </section>
</main>

<style>
  .page {
    min-height: calc(100vh - 9.5rem);
  }

  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 280px;
    gap: 0;
    min-height: calc(100vh - 9.5rem);
    border: 1px solid #1e3a5f;
    border-radius: 0.95rem;
    overflow: hidden;
    background: #08101c;
  }

  .main-column {
    min-width: 0;
  }

  .map-stage {
    position: relative;
    min-height: calc(100vh - 9.5rem);
    padding: 0;
    background: #070d18;
  }

  .map-badge {
    position: absolute;
    top: 0.9rem;
    left: 0.9rem;
    z-index: 2;
    display: grid;
    gap: 0.15rem;
    padding: 0.65rem 0.85rem;
    border-radius: 0.7rem;
    background: rgba(15, 24, 41, 0.92);
    border: 1px solid #1e3a5f;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.28);
  }

  .map-badge-label {
    color: #60a5fa;
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .map-badge strong {
    color: #dbeafe;
    font-size: 0.88rem;
  }

  .map-controls {
    position: absolute;
    left: 0.9rem;
    bottom: 0.9rem;
    z-index: 2;
    display: flex;
    gap: 0.4rem;
  }

  .map-controls button {
    width: 2.15rem;
    height: 2.15rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: rgba(15, 24, 41, 0.96);
    color: #60a5fa;
    font-weight: 700;
    cursor: default;
  }

  @media (max-width: 900px) {
    .layout {
      grid-template-columns: 1fr;
      min-height: auto;
    }

    .map-stage {
      min-height: 520px;
    }
  }
</style>
