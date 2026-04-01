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

  function updateSelectedZone<K extends 'stripWidth' | 'angle' | 'edgeRounds' | 'speed' | 'pattern' | 'edgeMowing'>(
    key: K,
    value: K extends 'edgeMowing' ? boolean : K extends 'pattern' ? 'stripe' | 'spiral' : number,
  ) {
    if (!$mapStore.selectedZoneId) return
    mapStore.updateZoneSettings($mapStore.selectedZoneId, { [key]: value })
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
      <div class="field-grid">
        <label>
          Streifenbreite
          <input
            type="number"
            min="0.05"
            max="1"
            step="0.01"
            value={zone.settings.stripWidth}
            on:input={(event) => updateSelectedZone('stripWidth', Number((event.currentTarget as HTMLInputElement).value))}
          />
        </label>

        <label>
          Winkel
          <input
            type="number"
            min="0"
            max="179"
            step="1"
            value={zone.settings.angle}
            on:input={(event) => updateSelectedZone('angle', Number((event.currentTarget as HTMLInputElement).value))}
          />
        </label>

        <label>
          Geschwindigkeit
          <input
            type="number"
            min="0.1"
            max="3"
            step="0.1"
            value={zone.settings.speed}
            on:input={(event) => updateSelectedZone('speed', Number((event.currentTarget as HTMLInputElement).value))}
          />
        </label>

        <label>
          Muster
          <select
            value={zone.settings.pattern}
            on:change={(event) => updateSelectedZone('pattern', (event.currentTarget as HTMLSelectElement).value as 'stripe' | 'spiral')}
          >
            <option value="stripe">Streifen</option>
            <option value="spiral">Spirale</option>
          </select>
        </label>
      </div>

      <label class="toggle-row">
        <input
          type="checkbox"
          checked={zone.settings.edgeMowing}
          on:change={(event) => updateSelectedZone('edgeMowing', (event.currentTarget as HTMLInputElement).checked)}
        />
        <span>Randmaehen aktiv</span>
      </label>

      <label>
        Randbahnen
        <input
          type="number"
          min="1"
          max="5"
          step="1"
          disabled={!zone.settings.edgeMowing}
          value={zone.settings.edgeRounds}
          on:input={(event) => updateSelectedZone('edgeRounds', Number((event.currentTarget as HTMLInputElement).value))}
        />
      </label>

      <div class="meta">Punkte: {zone.polygon.length}</div>
      <div class="meta">
        {zone.settings.pattern === 'stripe' ? 'Streifen' : 'Spirale'} · {zone.settings.angle}&deg;
        {#if zone.settings.edgeMowing}
          · Rand {zone.settings.edgeRounds}x
        {:else}
          · ohne Rand
        {/if}
      </div>
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
