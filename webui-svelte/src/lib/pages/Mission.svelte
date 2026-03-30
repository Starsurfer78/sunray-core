<script lang="ts">
  import { onMount } from 'svelte'
  import ZoneSelect from '../components/Mission/ZoneSelect.svelte'
  import MissionControls from '../components/Mission/MissionControls.svelte'
  import { getMapDocument } from '../api/rest'
  import { mapStore, type Point, type Zone } from '../stores/map'

  let selectedZoneIds: string[] = []
  let info = ''
  let error = ''
  let busy = false

  function normalizePoints(points: Array<[number, number]> | Point[] | undefined): Point[] {
    if (!points) return []
    return points.map((point) =>
      Array.isArray(point)
        ? { x: point[0], y: point[1] }
        : point,
    )
  }

  async function ensureZonesLoaded() {
    if ($mapStore.map.zones.length > 0) return

    busy = true
    error = ''
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
      info = 'Zonen aus Karte geladen'
    } catch (err) {
      error = err instanceof Error ? err.message : 'Zonen konnten nicht geladen werden'
    } finally {
      busy = false
    }
  }

  onMount(() => {
    void ensureZonesLoaded()
  })
</script>

<main class="page">
  <section class="head">
    <div>
      <span class="eyebrow">Mission</span>
      <h1>Zonen waehlen und maehen</h1>
    </div>
    <div class="status-card">
      <span class="label">Status</span>
      <strong>{busy ? 'Lade Zonen' : 'Bereit'}</strong>
      {#if error}
        <span class="error">{error}</span>
      {:else if info}
        <span class="ok">{info}</span>
      {:else}
        <span class="muted">Mission kann geplant werden</span>
      {/if}
    </div>
  </section>

  <section class="layout">
    <ZoneSelect zones={$mapStore.map.zones} bind:selectedZoneIds />
    <MissionControls zones={$mapStore.map.zones} {selectedZoneIds} />
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1rem;
  }

  .head {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 280px;
    gap: 1rem;
    align-items: start;
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

  h1 { font-size: 1.55rem; line-height: 1.05; }

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
    grid-template-columns: minmax(0, 1.2fr) minmax(280px, 0.8fr);
    gap: 1rem;
    align-items: start;
  }

  @media (max-width: 980px) {
    .head, .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
