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
    gap: 0.9rem;
    padding: 1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header,
  .actions {
    display: grid;
    gap: 0.6rem;
  }

  h2,
  p {
    margin: 0;
  }

  p,
  .meta {
    color: #9db3ab;
  }

  button {
    padding: 0.75rem 0.9rem;
    border: 0;
    border-radius: 0.8rem;
    background: #d69aa5;
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

  select {
    width: 100%;
    padding: 0.7rem 0.8rem;
    border: 1px solid rgba(152, 187, 170, 0.2);
    border-radius: 0.8rem;
    background: rgba(24, 38, 34, 0.85);
    color: #dce8e8;
  }
</style>
