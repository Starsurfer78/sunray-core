<script lang="ts">
  import { sendCmd } from '../../api/websocket'
  import { connection } from '../../stores/connection'
  import type { Zone } from '../../stores/map'

  export let zones: Zone[] = []
  export let selectedZoneIds: string[] = []

  function startSelected() {
    if (selectedZoneIds.length === 0) return
    sendCmd('startZones', { zones: selectedZoneIds })
  }

  function startAll() {
    sendCmd('start')
  }

  function dock() {
    sendCmd('dock')
  }

  function stop() {
    sendCmd('stop')
  }
</script>

<section class="panel">
  <header>
    <h2>Mission</h2>
    <p>Startet eine Vollflaeche oder nur die aktuell gewaehlten Zonen.</p>
  </header>

  <div class="actions">
    <button type="button" on:click={startSelected} disabled={!$connection.connected || selectedZoneIds.length === 0}>
      Gewaehlte Zonen maehen
    </button>
    <button type="button" on:click={startAll} disabled={!$connection.connected || zones.length === 0}>
      Alle Zonen / Standardstart
    </button>
    <button type="button" on:click={dock} disabled={!$connection.connected}>
      Dock
    </button>
    <button type="button" on:click={stop} disabled={!$connection.connected}>
      Stop
    </button>
  </div>

  <div class="meta">
    Auswahl: {selectedZoneIds.length} / {zones.length}
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header, .actions {
    display: grid;
    gap: 0.8rem;
  }

  h2, p {
    margin: 0;
  }

  p, .meta {
    color: #9db3ab;
  }

  button {
    padding: 0.8rem 1rem;
    border: 0;
    border-radius: 0.9rem;
    background: #8db8e8;
    color: #07110f;
    font-weight: 700;
    cursor: pointer;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
</style>
