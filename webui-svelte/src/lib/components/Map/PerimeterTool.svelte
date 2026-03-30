<script lang="ts">
  import { mapStore } from '../../stores/map'

  function activateTool() {
    mapStore.setTool('perimeter')
  }

  function removeLastPoint() {
    activateTool()
    mapStore.removeLastPoint()
  }

  function clearPerimeter() {
    activateTool()
    mapStore.clearActive()
  }
</script>

<section class="panel">
  <header>
    <h2>Perimeter</h2>
    <p>Aktiviere das Perimeter-Werkzeug und setze Punkte direkt im Gitter.</p>
  </header>

  <div class="actions">
    <button type="button" class:active={$mapStore.selectedTool === 'perimeter'} on:click={activateTool}>Perimeter aktivieren</button>
    <button type="button" on:click={removeLastPoint}>Letzten Punkt loeschen</button>
    <button type="button" on:click={clearPerimeter}>Perimeter leeren</button>
  </div>

  <div class="meta">
    Punkte: {$mapStore.map.perimeter.length}
    {#if $mapStore.selectedTool === 'perimeter'} · aktiv{/if}
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
    background: #9ecf80;
    color: #07110f;
    font-weight: 700;
    cursor: pointer;
  }
  button.active {
    outline: 2px solid rgba(230, 246, 255, 0.7);
    outline-offset: 1px;
  }
</style>
