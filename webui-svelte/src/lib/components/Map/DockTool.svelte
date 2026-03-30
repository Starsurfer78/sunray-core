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
    gap: 0.4rem;
    padding: 0.58rem 0.68rem;
    border-bottom: 1px solid #0f1829;
  }
  header, .actions { display: grid; gap: 0.3rem; }
  h2 { margin: 0; color: #7a8da8; font-size: 0.59rem; text-transform: uppercase; letter-spacing: 0.08em; font-weight: 500; }
  p, .meta { margin: 0; color: #64748b; font-size: 0.6rem; }
  button {
    padding: 0.38rem 0.5rem;
    border: 1px solid #d97706;
    border-radius: 0.4rem;
    background: #1c1200;
    color: #fbbf24;
    font-weight: 700;
    font-size: 0.64rem;
    cursor: pointer;
  }
  button.active {
    box-shadow: inset 0 0 0 1px #fde68a;
  }
</style>
