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
    <p>Wähl aus, welche Zonen der Roboter als nächstes mähen soll.</p>
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
          <span>{zone.name}</span>
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
    padding: 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
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

  p, .empty { color: #64748b; font-size: 0.84rem; }

  button {
    padding: 0.75rem 0.95rem;
    border: 1px solid #2563eb;
    border-radius: 0.6rem;
    background: #0c1a3a;
    color: #93c5fd;
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
    border-radius: 0.7rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
    color: #dce8e8;
  }

  .list label.selected {
    border-color: #2563eb;
    background: rgba(30, 58, 95, 0.45);
  }

  small {
    color: #64748b;
  }
</style>
