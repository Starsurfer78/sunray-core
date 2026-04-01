<script lang="ts">
  import { mapStore } from '../../stores/map'

  function onExclusionChange(event: Event) {
    const target = event.currentTarget as HTMLSelectElement
    const value = target.value === '' ? null : Number(target.value)
    mapStore.selectExclusion(value)
  }

  function activateTool() {
    mapStore.setTool('nogo')
  }

  function removeLastPoint() {
    activateTool()
    mapStore.removeLastPoint()
  }

  function clearActiveExclusion() {
    activateTool()
    mapStore.clearActive()
  }

</script>

<section class="panel">
  <header>
    <h2>NoGo-Zonen</h2>
    <p>NoGo-Zonen werden auf Basis der vorhandenen Karte als eigene Ausschlussflaechen angelegt.</p>
  </header>

  <div class="actions">
    <button type="button" on:click={() => mapStore.createExclusion()}>Neue NoGo-Zone</button>
    <button type="button" class:active={$mapStore.selectedTool === 'nogo'} on:click={activateTool}>NoGo-Werkzeug aktivieren</button>
    <button type="button" on:click={removeLastPoint}>Letzten Punkt loeschen</button>
    <button type="button" on:click={clearActiveExclusion}>Aktive NoGo-Zone loeschen</button>
  </div>

  <label>
    Aktive NoGo-Zone
    <select on:change={onExclusionChange}>
      <option value="">Keine</option>
      {#each $mapStore.map.exclusions as exclusion, index}
        <option value={index} selected={$mapStore.selectedExclusionIndex === index}>
          NoGo {index + 1} ({exclusion.length} Punkte)
        </option>
      {/each}
    </select>
  </label>

  {#if $mapStore.map.exclusions.length > 0}
    <div class="list">
      {#each $mapStore.map.exclusions as exclusion, index}
        <button
          type="button"
          class:list-item={true}
          class:selected={$mapStore.selectedExclusionIndex === index}
          on:click={() => mapStore.selectExclusion(index)}
        >
          <span>NoGo {index + 1}</span>
          <small>{exclusion.length} Punkte</small>
        </button>
      {/each}
    </div>
  {/if}

  <div class="meta">
    Gesamt: {$mapStore.map.exclusions.length}
    {#if $mapStore.selectedExclusionIndex !== null}
      · Punkte: {$mapStore.map.exclusions[$mapStore.selectedExclusionIndex]?.length ?? 0}
    {/if}
    {#if $mapStore.selectedTool === 'nogo'} · aktiv{/if}
  </div>
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
    border: 1px solid #dc2626;
    border-radius: 0.4rem;
    background: #450a0a;
    color: #fca5a5;
    font-weight: 700;
    font-size: 0.64rem;
    cursor: pointer;
  }
  button.active {
    box-shadow: inset 0 0 0 1px #fecdd3;
  }

  label {
    display: grid;
    gap: 0.25rem;
    color: #94a3b8;
    font-size: 0.6rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }

  select {
    width: 100%;
    padding: 0.32rem 0.45rem;
    border: 1px solid #1a2a40;
    border-radius: 0.4rem;
    background: #0a1020;
    color: #dce8e8;
    font-size: 0.7rem;
  }

  .list {
    display: grid;
    gap: 0.3rem;
    max-height: 160px;
    overflow: auto;
    padding-right: 0.2rem;
  }

  .list-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.5rem;
    padding: 0.32rem 0.45rem;
    border-radius: 0.4rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
    color: #dce8e8;
    font-size: 0.64rem;
  }

  .list-item small {
    color: #64748b;
    font-size: 0.6rem;
  }

  .list-item.selected {
    border-color: #dc2626;
    background: rgba(220, 38, 38, 0.12);
  }
</style>
