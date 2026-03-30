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
    gap: 0.4rem;
    padding: 0.58rem 0.68rem;
    border-bottom: 1px solid #0f1829;
  }
  header, .actions { display: grid; gap: 0.3rem; }
  h2 { margin: 0; color: #7a8da8; font-size: 0.59rem; text-transform: uppercase; letter-spacing: 0.08em; font-weight: 500; }
  p, .meta { margin: 0; color: #64748b; font-size: 0.6rem; }
  button {
    padding: 0.38rem 0.5rem;
    border: 1px solid #2563eb;
    border-radius: 0.4rem;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 700;
    font-size: 0.64rem;
    cursor: pointer;
  }
  button.active {
    box-shadow: inset 0 0 0 1px #93c5fd;
  }
</style>
