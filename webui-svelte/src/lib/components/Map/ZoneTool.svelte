<script lang="ts">
  import { mapStore } from '../../stores/map'

  function onZoneNameChange(event: Event) {
    const target = event.currentTarget as HTMLInputElement
    if ($mapStore.selectedZoneId) {
      mapStore.renameZone($mapStore.selectedZoneId, target.value)
    }
  }

  function activateTool() {
    mapStore.setTool('zone')
  }

  function removeLastPoint() {
    activateTool()
    mapStore.removeLastPoint()
  }

  function clearActiveZone() {
    activateTool()
    mapStore.clearActive()
  }

  function onZoneSelect(event: Event) {
    const value = (event.currentTarget as HTMLSelectElement).value
    mapStore.selectZone(value || null)
  }
</script>

<section class="panel">
  <header>
    <h2>Zonen</h2>
    <p>Neue Zone anlegen, aktivieren und ihre Polygonpunkte im Gitter setzen.</p>
  </header>

  <div class="actions">
    <button type="button" on:click={() => mapStore.createZone()}>Neue Zone</button>
    <button type="button" class:active={$mapStore.selectedTool === 'zone'} on:click={activateTool}>Zonenwerkzeug aktivieren</button>
    <button type="button" on:click={removeLastPoint}>Letzten Punkt löschen</button>
    <button type="button" on:click={clearActiveZone}>Aktive Zone löschen</button>
  </div>

  <label>
    Aktive Zone
    <select value={$mapStore.selectedZoneId ?? ''} on:change={onZoneSelect}>
      <option value="">Keine</option>
      {#each $mapStore.map.zones as zone}
        <option value={zone.id}>{zone.name}</option>
      {/each}
    </select>
  </label>

  {#if $mapStore.selectedZoneId}
    {@const zone = $mapStore.map.zones.find((entry) => entry.id === $mapStore.selectedZoneId)}
    {#if zone}
      <label>
        Name
        <input type="text" value={zone.name} on:input={onZoneNameChange} />
      </label>

      <div class="meta">Punkte: {zone.polygon.length}</div>
    {/if}
  {/if}
</section>

<style>
  .panel {
    display: grid;
    gap: 0.4rem;
    padding: 0.58rem 0.68rem;
    border-bottom: 1px solid #0f1829;
  }
  header, .actions { display: grid; gap: 0.3rem; }
  h2 { margin: 0; color: #7a8da8; font-size: 0.59rem; text-transform: uppercase; letter-spacing: 0.08em; font-weight: 500; }
  p, .meta { margin: 0; color: #64748b; font-size: 0.6rem; }
  button {
    padding: 0.38rem 0.5rem;
    border: 1px solid #0891b2;
    border-radius: 0.4rem;
    background: #082f49;
    color: #67e8f9;
    font-weight: 700;
    font-size: 0.64rem;
    cursor: pointer;
  }
  button.active {
    box-shadow: inset 0 0 0 1px #67e8f9;
  }
  .field-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.35rem;
  }
  label {
    display: grid;
    gap: 0.25rem;
    color: #94a3b8;
    font-size: 0.6rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }
  select, input {
    width: 100%;
    padding: 0.32rem 0.45rem;
    border: 1px solid #1a2a40;
    border-radius: 0.4rem;
    background: #0a1020;
    color: #dce8e8;
    font-size: 0.7rem;
  }
  .toggle-row {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    text-transform: none;
    letter-spacing: 0;
    font-size: 0.68rem;
  }
  .toggle-row input {
    width: auto;
  }
</style>
