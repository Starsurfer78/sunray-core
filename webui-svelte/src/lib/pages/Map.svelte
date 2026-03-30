<script lang="ts">
  import { onMount } from 'svelte'
  import MapCanvas from '../components/Map/MapCanvas.svelte'
  import PerimeterTool from '../components/Map/PerimeterTool.svelte'
  import DockTool from '../components/Map/DockTool.svelte'
  import ZoneTool from '../components/Map/ZoneTool.svelte'
  import NoGoZoneTool from '../components/Map/NoGoZoneTool.svelte'
  import { getMapDocument, saveMapDocument } from '../api/rest'
  import { mapStore, type Point, type Zone } from '../stores/map'

  let busy = false
  let error = ''
  let info = ''

  function normalizePoints(points: Array<[number, number]> | Point[] | undefined): Point[] {
    if (!points) return []
    return points.map((point) =>
      Array.isArray(point)
        ? { x: point[0], y: point[1] }
        : point,
    )
  }

  async function loadMap() {
    busy = true
    error = ''
    info = ''
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
      info = 'Karte geladen'
    } catch (err) {
      error = err instanceof Error ? err.message : 'Karte konnte nicht geladen werden'
    } finally {
      busy = false
    }
  }

  async function saveMap() {
    busy = true
    error = ''
    info = ''
    try {
      const payload = {
        perimeter: $mapStore.map.perimeter.map((point) => [point.x, point.y]),
        dock: $mapStore.map.dock.map((point) => [point.x, point.y]),
        // Preserve loaded/generated mow points until there is an explicit UI to edit them.
        mow: $mapStore.map.mow.map((point) => [point.x, point.y]),
        exclusions: $mapStore.map.exclusions.map((exclusion) => exclusion.map((point) => [point.x, point.y])),
        zones: $mapStore.map.zones.map((zone) => ({
          ...zone,
          polygon: zone.polygon.map((point) => [point.x, point.y]),
        })),
      }
      const result = await saveMapDocument(payload)
      if (!result.ok) {
        error = result.error ?? 'Karte konnte nicht gespeichert werden'
        return
      }
      mapStore.markSaved()
      info = 'Karte gespeichert'
    } catch (err) {
      error = err instanceof Error ? err.message : 'Karte konnte nicht gespeichert werden'
    } finally {
      busy = false
    }
  }

  onMount(() => {
    void loadMap()
  })
</script>

<main class="page">
  <section class="head">
    <span class="eyebrow">Karte</span>
    <div class="status-card">
      <span class="label">Status</span>
      <strong>{$mapStore.dirty ? 'Ungespeichert' : 'Synchron'}</strong>
      {#if error}
        <span class="error">{error}</span>
      {:else if info}
        <span class="ok">{info}</span>
      {:else}
        <span class="muted">Bereit</span>
      {/if}
    </div>
  </section>

  <section class="layout">
    <MapCanvas showHeader={false} />

    <aside class="tools">
      <div class="action-card">
        <span class="label">Karte</span>
        <strong>{$mapStore.dirty ? 'Ungespeichert' : 'Synchron'}</strong>
        <span class="muted">Werkzeuge und Speichern liegen bewusst in der rechten Sidebar.</span>
        <button type="button" disabled={busy} on:click={loadMap}>Neu laden</button>
        <button type="button" disabled={busy} on:click={saveMap}>Speichern</button>
      </div>
      <PerimeterTool />
      <DockTool />
      <ZoneTool />
      <NoGoZoneTool />
    </aside>
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1rem;
  }
  .head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
    gap: 1rem;
  }
  .eyebrow {
    display: inline-flex;
    align-items: center;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
    padding: 0.2rem 0;
  }
  .status-card {
    display: grid;
    gap: 0.25rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  .label {
    color: #7a8da8;
    font-size: 0.76rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }
  .muted { color: #94a3b8; font-size: 0.82rem; }
  .ok { color: #a8e2a1; font-size: 0.82rem; }
  .error { color: #f1aaaa; font-size: 0.82rem; }
  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1.9fr) 320px;
    gap: 1rem;
    align-items: start;
  }
  .tools {
    display: grid;
    gap: 1rem;
  }
  .action-card {
    display: grid;
    gap: 0.65rem;
    padding: 0.9rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  .action-card strong {
    color: #dbeafe;
    font-size: 1rem;
  }
  .action-card button {
    padding: 0.72rem 0.9rem;
    border: 1px solid #2563eb;
    border-radius: 0.6rem;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 700;
    cursor: pointer;
  }
  .muted {
    color: #94a3b8;
    font-size: 0.8rem;
  }
  .action-card button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
  @media (max-width: 980px) {
    .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
