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
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  header, .actions { display: grid; gap: 0.6rem; }
  h2, p { margin: 0; }
  p, .meta { color: #64748b; font-size: 0.84rem; }
  button {
    padding: 0.75rem 0.9rem;
    border: 1px solid #2563eb;
    border-radius: 0.6rem;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 700;
    cursor: pointer;
  }
  button.active {
    box-shadow: inset 0 0 0 1px #93c5fd;
  }
</style>
