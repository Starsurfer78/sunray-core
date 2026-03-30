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
    <button type="button" on:click={removeLastPoint}>Letzten Punkt loeschen</button>
    <button type="button" on:click={clearActiveZone}>Aktive Zone loeschen</button>
  </div>

  <label>
    Aktive Zone
    <select value={$mapStore.selectedZoneId ?? ''} on:change={onZoneSelect}>
      <option value="">Keine</option>
      {#each $mapStore.map.zones as zone}
        <option value={zone.id}>{zone.settings.name}</option>
      {/each}
    </select>
  </label>

  {#if $mapStore.selectedZoneId}
    {@const zone = $mapStore.map.zones.find((entry) => entry.id === $mapStore.selectedZoneId)}
    {#if zone}
      <label>
        Name
        <input type="text" value={zone.settings.name} on:input={onZoneNameChange} />
      </label>
      <div class="meta">Punkte: {zone.polygon.length}</div>
    {/if}
  {/if}
</section>

<style>
  .panel {
    display: grid;
    gap: 0.9rem;
    padding: 1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }
  header, .actions { display: grid; gap: 0.6rem; }
  h2, p { margin: 0; }
  p, .meta { color: #9db3ab; }
  button {
    padding: 0.75rem 0.9rem;
    border: 0;
    border-radius: 0.8rem;
    background: #87b7e8;
    color: #07110f;
    font-weight: 700;
    cursor: pointer;
  }
  button.active {
    outline: 2px solid rgba(230, 246, 255, 0.7);
    outline-offset: 1px;
  }
  label {
    display: grid;
    gap: 0.35rem;
    color: #c8d9d2;
  }
  select, input {
    width: 100%;
    padding: 0.7rem 0.8rem;
    border: 1px solid rgba(152, 187, 170, 0.2);
    border-radius: 0.8rem;
    background: rgba(24, 38, 34, 0.85);
    color: #dce8e8;
  }
</style>
