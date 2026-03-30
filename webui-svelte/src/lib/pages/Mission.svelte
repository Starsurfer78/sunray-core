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
  <section class="hero">
    <div>
      <span class="eyebrow">Mission</span>
      <h1>Zonen waehlen und maehen</h1>
      <p>Der Missionsblock nutzt die vorhandenen Kartenzonen und triggert die Fahrbefehle ueber die bestehende WebSocket-Schnittstelle.</p>
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
    color: #b4d37e;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.8rem;
  }

  h1, p {
    margin: 0;
  }

  h1 {
    font-size: clamp(2rem, 4vw, 3.2rem);
    line-height: 1;
    margin-bottom: 0.6rem;
  }

  p {
    color: #a5bab4;
  }

  .status-card {
    display: grid;
    gap: 0.35rem;
    padding: 1.2rem;
    border-radius: 1rem;
    background: linear-gradient(160deg, rgba(111, 147, 67, 0.3), rgba(23, 37, 33, 0.9));
    border: 1px solid rgba(173, 213, 116, 0.2);
  }

  .label { color: #d0d6b2; }
  .muted { color: #93a59e; }
  .ok { color: #a8e2a1; }
  .error { color: #f1aaaa; }

  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1.2fr) minmax(280px, 0.8fr);
    gap: 1rem;
    align-items: start;
  }

  @media (max-width: 980px) {
    .hero, .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
