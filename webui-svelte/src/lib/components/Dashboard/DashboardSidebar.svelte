<script lang="ts">
  import { createEventDispatcher, onMount } from "svelte";
  import MissionWidget from "./MissionWidget.svelte";
  import RobotControls from "../RobotControls.svelte";
  import { batteryPercent, gpsQuality, telemetry } from "../../stores/telemetry";
  import { connection } from "../../stores/connection";
  import { mapStore } from "../../stores/map";
  import {
    getPreflightChecks,
    getPrimaryNotice,
    humanizeReason,
  } from "../../utils/robotUi";

  export let sidebarCollapsed = false;

  const dispatch = createEventDispatcher();

  let nowMs = Date.now();

  $: preflight = getPreflightChecks($telemetry, $connection, $mapStore.map, nowMs);
  $: startRelease = {
    allowed: preflight.every((check) => check.ok),
    blockers: preflight.filter((check) => !check.ok),
  };
  $: primaryNotice = getPrimaryNotice(
    $telemetry,
    $connection,
    $mapStore.map,
    nowMs,
  );
  $: startHint =
    startRelease.blockers[0]?.hint ??
    "Startfreigabe noch nicht erteilt.";
  $: reasonText = humanizeReason($telemetry.event_reason);
  $: batteryValue = `${$telemetry.battery_v.toFixed(1)} V · ${$batteryPercent}%`;

  const toneClass = (tone: string) => `tone-${tone}`;

  const formatConnectionState = () => {
    if (!$connection.connected) return "Getrennt";
    const age = Math.max(0, Math.round((nowMs - $connection.lastSeen) / 1000));
    if (age <= 5) return "Live";
    return `Alt (${age}s)`;
  };

  const formatMapState = () =>
    $mapStore.map.perimeter.length >= 3
      ? `${$mapStore.map.perimeter.length} P · Dock ${$mapStore.map.dock.length}`
      : "Keine freigegebene Karte";

  const formatErrorState = () => {
    if ($telemetry.op === "Error") {
      return $telemetry.error_code || reasonText;
    }
    return $telemetry.motor_err || $telemetry.lift || $telemetry.bumper_l || $telemetry.bumper_r
      ? "Sensor- oder Motorblocker aktiv"
      : "Unauffaellig";
  };

  onMount(() => {
    const interval = setInterval(() => {
      nowMs = Date.now();
    }, 1000);

    return () => clearInterval(interval);
  });
</script>

<svelte:head>
  <!-- keep empty; component-only -->
</svelte:head>

<aside class="sidebar" class:collapsed={sidebarCollapsed}>
  <button
    class="collapse-btn"
    class:collapsed={sidebarCollapsed}
    title={sidebarCollapsed ? "Sidebar ausklappen" : "Sidebar einklappen"}
    on:click={() => dispatch("toggle")}
  >
    <span class="collapse-icon">{sidebarCollapsed ? "›" : "‹"}</span>
  </button>

  <div class="sr-side" class:hidden={sidebarCollapsed}>
    <section class="panel">
      <div class="panel-head">
        <span class="label">Betrieb</span>
        <strong>{$telemetry.op}</strong>
        <span class="muted">Phase {$telemetry.state_phase || "idle"}</span>
      </div>

      <div class={`notice ${toneClass(primaryNotice.tone)}`}>
        <span class="notice-title">{primaryNotice.title}</span>
        <span>{primaryNotice.detail}</span>
        <strong>{primaryNotice.action}</strong>
      </div>
    </section>

    <section class="panel">
      <div class="section-title">Startfreigabe</div>
      <div class="release-badge" class:ok={startRelease.allowed} class:blocked={!startRelease.allowed}>
        {#if startRelease.allowed}
          <strong>Freigabe erteilt</strong>
          <span>Start ist aus Sicht von Verbindung, GPS, Akku, Karte, Dock und Fehlerstatus moeglich.</span>
        {:else}
          <strong>Start blockiert</strong>
          <span>{startHint}</span>
        {/if}
      </div>

      <div class="check-list">
        {#each preflight as check}
          <div class="check-row" class:ok={check.ok} class:blocked={!check.ok}>
            <div class="check-main">
              <span>{check.label}</span>
              <strong>{check.value}</strong>
            </div>
            <span class="check-hint">{check.hint}</span>
          </div>
        {/each}
      </div>
    </section>

    <section class="panel">
      <div class="section-title">Naechste Aktion</div>
      <div class="summary-grid">
        <div class="summary-card">
          <span class="metric-label">GPS</span>
          <strong>{$gpsQuality}</strong>
          <span class="muted">Acc {$telemetry.gps_acc.toFixed(2)} m</span>
        </div>
        <div class="summary-card">
          <span class="metric-label">Akku</span>
          <strong>{batteryValue}</strong>
          <span class="muted">Dock {$telemetry.charge_v.toFixed(1)} V</span>
        </div>
        <div class="summary-card">
          <span class="metric-label">Verbindung</span>
          <strong>{formatConnectionState()}</strong>
          <span class="muted">MCU {$telemetry.mcu_v || "---"}</span>
        </div>
        <div class="summary-card">
          <span class="metric-label">Karte</span>
          <strong>{formatMapState()}</strong>
          <span class="muted">Grund {reasonText}</span>
        </div>
        <div class="summary-card wide">
          <span class="metric-label">Recovery / Fehler</span>
          <strong>{formatErrorState()}</strong>
          <span class="muted">
            {$telemetry.resume_target
              ? `Resume-Ziel ${$telemetry.resume_target}`
              : "Kein aktives Resume-Ziel"}
          </span>
        </div>
      </div>
    </section>

    <MissionWidget zones={$mapStore.map.zones} />

    <RobotControls startAllowed={startRelease.allowed} {startHint} />
  </div>
</aside>

<style>
  .sidebar {
    position: absolute;
    top: 0;
    right: 0;
    bottom: 0;
    width: 275px;
    z-index: 3;
    padding-left: 0.55rem;
  }

  .sidebar.collapsed {
    width: 0;
  }

  .sr-side {
    display: flex;
    flex-direction: column;
    background: #0a1020;
    border: 1px solid #1e3a5f;
    border-radius: 0.75rem;
    overflow: hidden;
    height: 100%;
    box-shadow: 0 18px 40px rgba(0, 0, 0, 0.28);
  }

  .sr-side.hidden {
    display: none;
  }

  .collapse-btn {
    position: absolute;
    left: calc(0.55rem - 14px);
    top: 50%;
    transform: translateY(-50%);
    z-index: 10;
    width: 14px;
    height: 44px;
    background: #0f1829 !important;
    border: 1px solid #1e3a5f;
    border-right: none;
    color: #475569 !important;
    cursor: pointer;
    border-radius: 6px 0 0 6px;
    font-size: 14px;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0;
    transition: color 0.2s;
  }

  .collapse-btn:hover {
    color: #60a5fa !important;
  }

  .collapse-btn.collapsed {
    left: calc(0.55rem - 14px);
    border-right: 1px solid #1e3a5f;
    border-left: none;
    border-radius: 0 6px 6px 0;
  }

  .collapse-icon {
    display: inline-block;
  }

  .panel {
    display: grid;
    gap: 0.45rem;
    padding: 0.62rem 0.7rem;
    border-bottom: 1px solid #0f1829;
  }

  .panel-head {
    display: grid;
    gap: 0.12rem;
  }

  .label,
  .section-title,
  .metric-label {
    color: #7a8da8;
    font-size: 0.59rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .section-title {
    margin-bottom: 0.05rem;
  }

  strong {
    color: #dbeafe;
    font-size: 0.76rem;
  }

  .panel-head strong {
    font-size: 1rem;
    color: #60a5fa;
  }

  .muted {
    color: #94a3b8;
    font-size: 0.64rem;
  }

  .notice,
  .release-badge {
    display: grid;
    gap: 0.22rem;
    padding: 0.65rem;
    border-radius: 0.6rem;
    border: 1px solid #1e3a5f;
    background: #0f1829;
  }

  .notice-title {
    font-size: 0.74rem;
    font-weight: 700;
  }

  .tone-success {
    border-color: #166534;
    background: #052e16;
    color: #86efac;
  }

  .tone-warning {
    border-color: #92400e;
    background: #3b2305;
    color: #fbbf24;
  }

  .tone-error {
    border-color: #7f1d1d;
    background: #450a0a;
    color: #fca5a5;
  }

  .tone-info {
    border-color: #1d4ed8;
    background: #0c1a3a;
    color: #93c5fd;
  }

  .release-badge.ok {
    border-color: #166534;
    background: rgba(5, 46, 22, 0.8);
  }

  .release-badge.blocked {
    border-color: #92400e;
    background: rgba(59, 35, 5, 0.82);
  }

  .check-list {
    display: grid;
    gap: 0.35rem;
  }

  .check-row {
    display: grid;
    gap: 0.12rem;
    padding: 0.46rem 0.52rem;
    border-radius: 0.48rem;
    border: 1px solid #1a2a40;
    background: #0f1829;
  }

  .check-row.ok {
    border-color: #166534;
  }

  .check-row.blocked {
    border-color: #92400e;
  }

  .check-main {
    display: flex;
    justify-content: space-between;
    gap: 0.5rem;
    font-size: 0.66rem;
    color: #cbd5e1;
  }

  .check-hint {
    color: #94a3b8;
    font-size: 0.62rem;
    line-height: 1.35;
  }

  .summary-grid {
    display: grid;
    gap: 0.35rem;
  }

  .summary-card {
    display: grid;
    gap: 0.1rem;
    padding: 0.48rem 0.52rem;
    border-radius: 0.48rem;
    border: 1px solid #1a2a40;
    background: #0f1829;
  }

  .summary-card.wide {
    gap: 0.18rem;
  }
</style>
