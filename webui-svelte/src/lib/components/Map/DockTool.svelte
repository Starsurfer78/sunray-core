<script lang="ts">
  import { mapStore } from '../../stores/map'

  function activateTool() {
    mapStore.setTool('dock')
  }

  function clearDock() {
    activateTool()
    mapStore.clearActive()
  }
</script>

<section class="panel">
  <header>
    <h2>Dock</h2>
    <p>Setzt genau einen Dockpunkt. Ein Klick im Gitter ersetzt die alte Position.</p>
  </header>

  <div class="actions">
    <button type="button" class:active={$mapStore.selectedTool === 'dock'} on:click={activateTool}>Dock aktivieren</button>
    <button type="button" on:click={clearDock}>Dock loeschen</button>
  </div>

  <div class="meta">
    Punkte: {$mapStore.map.dock.length}
    {#if $mapStore.selectedTool === 'dock'} · aktiv{/if}
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 0.9rem;
    padding: 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  header, .actions { display: grid; gap: 0.6rem; }
  h2, p { margin: 0; }
  p, .meta { color: #64748b; font-size: 0.84rem; }
  button {
    padding: 0.75rem 0.9rem;
    border: 1px solid #d97706;
    border-radius: 0.6rem;
    background: #1c1200;
    color: #fbbf24;
    font-weight: 700;
    cursor: pointer;
  }
  button.active {
    box-shadow: inset 0 0 0 1px #fde68a;
  }
</style>
