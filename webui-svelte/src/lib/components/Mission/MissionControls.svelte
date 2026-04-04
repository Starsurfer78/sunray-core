<script lang="ts">
  import { sendCmd } from '../../api/websocket'
  import { connection } from '../../stores/connection'
  import type { Mission } from '../../stores/missions'
  import type { Zone } from '../../stores/map'

  export let zones: Zone[] = []
  export let mission: Mission | null = null
  export let selectedZoneIds: string[] = []

  function startSelected() {
    if (selectedZoneIds.length === 0) return
    sendCmd('startZones', { zones: selectedZoneIds })
  }

  function startMission() {
    if (!mission) return
    sendCmd('start', { missionId: mission.id })
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
    <p>Startet eine Vollflaeche oder nur die aktuell gewählten Zonen.</p>
  </header>

  <div class="actions">
    <button type="button" on:click={startMission} disabled={!$connection.connected || !mission}>
      Mission starten
    </button>
    <button type="button" on:click={startSelected} disabled={!$connection.connected || selectedZoneIds.length === 0}>
      Gewählte Zonen mähen
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
    {#if mission}
      Mission: {mission.name}<br />
    {/if}
    Auswahl: {selectedZoneIds.length} / {zones.length}
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  header, .actions {
    display: grid;
    gap: 0.8rem;
  }

  h2, p {
    margin: 0;
  }

  p, .meta { color: #64748b; font-size: 0.84rem; }

  button {
    padding: 0.8rem 1rem;
    border: 1px solid #2563eb;
    border-radius: 0.6rem;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 700;
    cursor: pointer;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
</style>
