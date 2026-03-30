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
  <section class="hero">
    <div>
      <span class="eyebrow">Karte</span>
      <h1>Perimeter, Dock und Zonen</h1>
      <p>
        Der Karten-MVP nutzt nur ein lokales Gitter. Kein OpenStreetMap, kein
        Satellitenlayer, nur die Arbeitsflaeche fuer Alfred.
      </p>
    </div>
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

  <div class="top-actions">
    <button type="button" disabled={busy} on:click={loadMap}>Neu laden</button>
    <button type="button" disabled={busy} on:click={saveMap}>Speichern</button>
  </div>

  <section class="layout">
    <MapCanvas />

    <aside class="tools">
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
    gap: 1.2rem;
  }
  .hero {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 280px;
    gap: 1rem;
  }
  .eyebrow {
    display: inline-block;
    margin-bottom: 0.6rem;
    color: #8cc1f0;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.8rem;
  }
  h1, p { margin: 0; }
  h1 {
    font-size: clamp(2rem, 4vw, 3.2rem);
    line-height: 1;
    margin-bottom: 0.6rem;
  }
  p { color: #a5bab4; }
  .status-card {
    display: grid;
    gap: 0.35rem;
    padding: 1.2rem;
    border-radius: 1rem;
    background: linear-gradient(160deg, rgba(86, 128, 178, 0.3), rgba(23, 37, 33, 0.9));
    border: 1px solid rgba(143, 192, 240, 0.2);
  }
  .label { color: #d0d6b2; }
  .muted { color: #93a59e; }
  .ok { color: #a8e2a1; }
  .error { color: #f1aaaa; }
  .top-actions {
    display: flex;
    gap: 0.8rem;
    flex-wrap: wrap;
  }
  .top-actions button {
    padding: 0.8rem 1rem;
    border: 0;
    border-radius: 0.9rem;
    background: #7db8ea;
    color: #07110f;
    font-weight: 700;
    cursor: pointer;
  }
  .top-actions button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1.45fr) minmax(280px, 0.75fr);
    gap: 1rem;
    align-items: start;
  }
  .tools {
    display: grid;
    gap: 1rem;
  }
  @media (max-width: 980px) {
    .hero, .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
