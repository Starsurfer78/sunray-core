<script lang="ts">
  import { sendCmd } from "../../api/websocket";
  import { connection } from "../../stores/connection";
  import type { Mission } from "../../stores/missions";
  import type { Zone } from "../../stores/map";
  import type { Telemetry } from "../../api/types";

  export let zones: Zone[] = [];
  export let mission: Mission | null = null;
  export let selectedZoneIds: string[] = [];
  export let telemetry: Telemetry | null = null; // N6.3: for resume button

  function startSelected() {
    if (selectedZoneIds.length === 0) return;
    sendCmd("startZones", { zones: selectedZoneIds });
  }

  function startMission() {
    if (!mission) return;
    sendCmd("start", { missionId: mission.id });
  }

  function startAll() {
    sendCmd("start");
  }

  function resumeMission() {
    sendCmd("resume");
  }

  function dock() {
    sendCmd("dock");
  }

  function stop() {
    sendCmd("stop");
  }
</script>

<section class="panel">
  <header>
    <h2>Mission</h2>
    <p>Startet eine Vollflaeche oder nur die aktuell gewählten Zonen.</p>
  </header>

  <div class="actions">
    <button
      type="button"
      on:click={startMission}
      disabled={!$connection.connected || !mission}
    >
      Mission starten
    </button>
    <button
      type="button"
      on:click={startSelected}
      disabled={!$connection.connected || selectedZoneIds.length === 0}
    >
      Gewählte Zonen mähen
    </button>
    <button
      type="button"
      on:click={startAll}
      disabled={!$connection.connected || zones.length === 0}
    >
      Alle Zonen / Standardstart
    </button>
    {#if telemetry?.has_interrupted_mission}
      <button
        type="button"
        class="resume-btn"
        on:click={resumeMission}
        disabled={!$connection.connected}
      >
        Auftrag fortsetzen
      </button>
    {/if}
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

  header,
  .actions {
    display: grid;
    gap: 0.8rem;
  }

  h2,
  p {
    margin: 0;
  }

  p,
  .meta {
    color: #64748b;
    font-size: 0.84rem;
  }

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

  .resume-btn {
    border-color: #16a34a;
    background: #052e16;
    color: #4ade80;
  }
</style>
