<script lang="ts">
  import { onDestroy } from "svelte";
  import { batteryPercent, telemetry } from "../stores/telemetry";
  import { connection } from "../stores/connection";
  import { mapInfoOpen } from "../stores/mapInfo";
  import { mappingTestMode } from "../stores/mapUi";
  import { CONNECTION_FRESH_MS, clamp } from "../utils/mapHelpers";

  export let currentView:
    | "dashboard"
    | "history"
    | "map"
    | "mission"
    | "settings";

  let nowMs = Date.now();
  let panelX = 16;
  let panelY = 84;
  let dragging = false;
  let dragOffsetX = 0;
  let dragOffsetY = 0;

  $: connectionAgeSeconds =
    $connection.lastSeen > 0
      ? Math.max(0, Math.round((nowMs - $connection.lastSeen) / 1000))
      : null;
  $: connectionFresh =
    $connection.connected &&
    nowMs - $connection.lastSeen <= CONNECTION_FRESH_MS;
  $: gpsStatus =
    $telemetry.gps_sol === 4
      ? "RTK Fix"
      : $telemetry.gps_sol === 5
        ? "RTK Float"
        : $telemetry.gps_text || "invalid";
  $: gpsSignal = $telemetry.gps_text || "—";
  $: connectionState = !$connection.connected
    ? "offline"
    : connectionFresh
      ? "live"
      : connectionAgeSeconds !== null
        ? `alt (${connectionAgeSeconds}s)`
        : "verbunden";
  $: missionIndex =
    $telemetry.mission_zone_count > 0
      ? `${Math.max(1, $telemetry.mission_zone_index || 1)}/${$telemetry.mission_zone_count}`
      : "—";
  $: dockContact = $telemetry.charger_connected ? "ja" : "nein";
  $: resumeTarget = $telemetry.resume_target || "—";
  $: systemHealth = $telemetry.runtime_health || "ok";
  $: statePhase = $telemetry.state_phase || "—";

  function onHeaderPointerDown(event: PointerEvent) {
    dragging = true;
    dragOffsetX = event.clientX - panelX;
    dragOffsetY = event.clientY - panelY;
    (event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
  }

  function onHeaderPointerMove(event: PointerEvent) {
    if (!dragging) return;
    const panelWidth = 384;
    const panelHeight = 280;
    panelX = clamp(
      event.clientX - dragOffsetX,
      8,
      window.innerWidth - panelWidth - 8,
    );
    panelY = clamp(
      event.clientY - dragOffsetY,
      72,
      window.innerHeight - panelHeight - 8,
    );
  }

  function onHeaderPointerUp(event: PointerEvent) {
    dragging = false;
    try {
      (event.currentTarget as HTMLElement).releasePointerCapture(
        event.pointerId,
      );
    } catch {
      // ignore
    }
  }

  function closePanel() {
    mapInfoOpen.set(false);
  }

  const interval = setInterval(() => {
    nowMs = Date.now();
  }, 1000);

  onDestroy(() => {
    clearInterval(interval);
  });
</script>

{#if $mapInfoOpen}
  <div class="global-info-panel" style={`left:${panelX}px; top:${panelY}px;`}>
    <div
      class="global-info-head"
      role="toolbar"
      tabindex="0"
      aria-label="Infofenster verschieben"
      on:pointerdown={onHeaderPointerDown}
      on:pointermove={onHeaderPointerMove}
      on:pointerup={onHeaderPointerUp}
    >
      <strong>Info</strong>
      <div class="global-info-actions">
        <span>ziehen zum Verschieben</span>
        <button
          type="button"
          class="global-info-close"
          on:click|stopPropagation={closePanel}
        >
          Schließen
        </button>
      </div>
    </div>
    <div class="global-info-grid">
      <div class="global-info-card">
        <span class="global-info-label">GPS / RTK</span>
        <strong>{gpsStatus}</strong>
        <span
          >Acc: {$telemetry.gps_acc > 0
            ? `${$telemetry.gps_acc.toFixed(2)} m`
            : "—"}</span
        >
        <span>Qualität: {gpsSignal} / sol {$telemetry.gps_sol || "—"}</span>
        <span>EKF: {$telemetry.ekf_health || "—"}</span>
        <span
          >Link: {connectionAgeSeconds !== null
            ? `${connectionAgeSeconds}s`
            : "—"}</span
        >
      </div>

      <div class="global-info-card">
        <span class="global-info-label">Position</span>
        <strong>{connectionState}</strong>
        <span>Pos x: {$telemetry.x.toFixed(2)} m</span>
        <span>Pos y: {$telemetry.y.toFixed(2)} m</span>
        <span>Phase: {statePhase}</span>
        <span>Resume: {resumeTarget}</span>
        <span>Idx: {missionIndex}</span>
      </div>

      <div class="global-info-card">
        <span class="global-info-label">Akku</span>
        <strong>{$batteryPercent}%</strong>
        <span>Voltage: {$telemetry.battery_v.toFixed(2)} V</span>
        <span>Current: {$telemetry.charge_a.toFixed(2)} A</span>
        <span>Dock: {$telemetry.charge_v.toFixed(2)} V</span>
        <span>Kontakt: {dockContact}</span>
      </div>

      <div class="global-info-card">
        <span class="global-info-label">System</span>
        <strong>{$telemetry.pi_v || "—"}</strong>
        <span>MCU: {$telemetry.mcu_v || "—"}</span>
        <span>Health: {systemHealth}</span>
        <span>Ansicht: {currentView}</span>
        {#if currentView === "map"}
          <span>Mode: {$mappingTestMode ? "Testmodus aktiv" : "Normal"}</span>
          <button
            type="button"
            class="info-toggle"
            class:active={$mappingTestMode}
            on:click={() => mappingTestMode.update((v) => !v)}
          >
            {$mappingTestMode ? "Testmodus aktiv" : "Testmodus für Mauspunkte"}
          </button>
        {/if}
      </div>
    </div>
  </div>
{/if}

<style>
  .global-info-panel {
    position: fixed;
    z-index: 30;
    width: min(24rem, calc(100% - 2rem));
    padding: 0.7rem;
    border-radius: 0.8rem;
    border: 1px solid #1e3a5f;
    background: rgba(7, 13, 24, 0.92);
    box-shadow: 0 14px 30px rgba(0, 0, 0, 0.34);
    backdrop-filter: blur(10px);
  }

  .global-info-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.6rem;
    margin-bottom: 0.55rem;
    padding: 0.1rem 0.1rem 0.45rem;
    border-bottom: 1px solid rgba(30, 58, 95, 0.75);
    cursor: grab;
    user-select: none;
  }

  .global-info-head:active {
    cursor: grabbing;
  }

  .global-info-head strong {
    color: #dbeafe;
    font-size: 0.78rem;
  }

  .global-info-head span {
    color: #7a8da8;
    font-size: 0.62rem;
  }

  .global-info-actions {
    display: flex;
    align-items: center;
    gap: 0.45rem;
  }

  .global-info-close {
    padding: 0.28rem 0.48rem;
    border-radius: 0.42rem;
    border: 1px solid #1e3a5f;
    background: #0f1829;
    color: #bfdbfe;
    font-size: 0.62rem;
    cursor: pointer;
  }

  .global-info-close:hover {
    border-color: #60a5fa;
    color: #dbeafe;
  }

  .global-info-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.5rem;
  }

  .global-info-card {
    display: grid;
    gap: 0.2rem;
    padding: 0.6rem;
    border-radius: 0.6rem;
    border: 1px solid rgba(30, 58, 95, 0.75);
    background: rgba(15, 24, 41, 0.72);
  }

  .global-info-card strong {
    color: #e2e8f0;
    font-size: 0.78rem;
  }

  .global-info-card span {
    color: #a9bacd;
    font-size: 0.66rem;
    line-height: 1.35;
  }

  .global-info-label {
    color: #7a8da8 !important;
    font-size: 0.57rem !important;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .info-toggle {
    margin-top: 0.25rem;
    padding: 0.42rem 0.55rem;
    border-radius: 0.45rem;
    border: 1px solid #1e3a5f;
    background: #0f1829;
    color: #bfdbfe;
    font-size: 0.64rem;
    font-weight: 600;
    cursor: pointer;
    text-align: left;
  }

  .info-toggle.active {
    border-color: #d97706;
    background: rgba(28, 18, 0, 0.72);
    color: #fbbf24;
  }
</style>
