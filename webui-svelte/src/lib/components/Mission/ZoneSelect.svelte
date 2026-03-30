<script lang="ts">
  import type { Zone } from '../../stores/map'

  export let zones: Zone[] = []
  export let selectedZoneIds: string[] = []

  function toggleZone(zoneId: string) {
    if (selectedZoneIds.includes(zoneId)) {
      selectedZoneIds = selectedZoneIds.filter((id) => id !== zoneId)
    } else {
      selectedZoneIds = [...selectedZoneIds, zoneId]
    }
  }

  function selectAll() {
    selectedZoneIds = zones.map((zone) => zone.id)
  }

  function clearSelection() {
    selectedZoneIds = []
  }
</script>

<section class="panel">
  <header>
    <h2>Zonenwahl</h2>
    <p>Waehl aus, welche Zonen der Roboter als naechstes maehen soll.</p>
  </header>

  <div class="toolbar">
    <button type="button" on:click={selectAll} disabled={zones.length === 0}>Alle</button>
    <button type="button" on:click={clearSelection} disabled={selectedZoneIds.length === 0}>Keine</button>
  </div>

  {#if zones.length === 0}
    <div class="empty">Noch keine Zonen in der Karte vorhanden.</div>
  {:else}
    <div class="list">
      {#each zones as zone}
        <label class:selected={selectedZoneIds.includes(zone.id)}>
          <input
            type="checkbox"
            checked={selectedZoneIds.includes(zone.id)}
            on:change={() => toggleZone(zone.id)}
          />
          <span>{zone.settings.name}</span>
          <small>{zone.polygon.length} Punkte</small>
        </label>
      {/each}
    </div>
  {/if}
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header, .toolbar, .list {
    display: grid;
    gap: 0.8rem;
  }

  .toolbar {
    grid-template-columns: repeat(2, minmax(120px, max-content));
  }

  h2, p {
    margin: 0;
  }

  p, .empty {
    color: #9db3ab;
  }

  button {
    padding: 0.75rem 0.95rem;
    border: 0;
    border-radius: 0.8rem;
    background: #9fd17d;
    color: #07110f;
    font-weight: 700;
    cursor: pointer;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .list label {
    display: grid;
    grid-template-columns: auto 1fr auto;
    gap: 0.7rem;
    align-items: center;
    padding: 0.8rem 0.9rem;
    border-radius: 0.9rem;
    background: rgba(24, 38, 34, 0.8);
    border: 1px solid rgba(152, 187, 170, 0.12);
    color: #dce8e8;
  }

  .list label.selected {
    border-color: rgba(173, 219, 141, 0.4);
    background: rgba(55, 80, 43, 0.38);
  }

  small {
    color: #8fa59f;
  }
</style>
