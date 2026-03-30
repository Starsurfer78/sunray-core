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
  <section class="title-row">
    <div class="title-copy">
      <span class="eyebrow">Dashboard</span>
      <h1>Betriebsueberblick</h1>
    </div>
    <div class="dashboard-meta">
      <div class="meta-pill">
        <span class="meta-label">Status</span>
        <strong>{$telemetry.op}</strong>
      </div>
      <div class="meta-pill">
        <span class="meta-label">Karte</span>
        <strong>{mapStatus}</strong>
      </div>
    </div>
  </section>

  <section class="layout">
    <div class="main-column">
      <div class="map-stage">
        <MapCanvas showHeader={false} showViewportActions={false} interactive={false} height={680} />
      </div>
    </div>

    <DashboardSidebar />
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1rem;
  }

  .title-row {
    display: flex;
    align-items: end;
    justify-content: space-between;
    gap: 1rem;
    flex-wrap: wrap;
  }

  .title-copy {
    display: grid;
    gap: 0.1rem;
  }

  .dashboard-meta {
    display: flex;
    gap: 0.65rem;
    flex-wrap: wrap;
  }

  .meta-pill {
    display: grid;
    gap: 0.15rem;
    min-width: 8.5rem;
    padding: 0.75rem 0.9rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .meta-label {
    color: #7a8da8;
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .meta-pill strong {
    color: #dbeafe;
    font-size: 0.95rem;
  }

  .eyebrow {
    display: inline-block;
    margin-bottom: 0.35rem;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }

  h1 { margin: 0; }

  h1 { font-size: 1.7rem; line-height: 1.05; }

  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1.9fr) 320px;
    gap: 1rem;
    align-items: start;
  }

  .main-column {
    min-width: 0;
  }

  .map-stage {
    min-height: 680px;
  }

  @media (max-width: 900px) {
    .layout {
      grid-template-columns: 1fr;
    }

    .map-stage {
      min-height: 520px;
    }
  }
</style>
