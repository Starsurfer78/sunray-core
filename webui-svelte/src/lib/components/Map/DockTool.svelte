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
    background: #e1c57e;
    color: #07110f;
    font-weight: 700;
    cursor: pointer;
  }
  button.active {
    outline: 2px solid rgba(230, 246, 255, 0.7);
    outline-offset: 1px;
  }
</style>
