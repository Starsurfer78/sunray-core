<script lang="ts">
  import { createEventDispatcher } from "svelte";
  import RobotControls from "../RobotControls.svelte";
  import {
    gpsQuality,
    batteryPercent,
    telemetry,
  } from "../../stores/telemetry";

  export let sidebarCollapsed = false;

  const dispatch = createEventDispatcher();

  const sensorStateClass = (value: boolean) => (value ? "hot" : "idle");
</script>

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
        <span class="label">Status</span>
        <strong>{$telemetry.op}</strong>
        <span class="muted">Phase {$telemetry.state_phase || "idle"}</span>
        <span class="status-pos"
          >X {$telemetry.x.toFixed(2)} m · Y {$telemetry.y.toFixed(2)} m</span
        >
      </div>
    </section>

    <section class="panel">
      <div class="section-title">GPS</div>
      <div class="gps-card">
        <div class="gps-head">
          <strong class="gps-fix">{$gpsQuality}</strong>
          <span class="gps-sol">acc {$telemetry.gps_acc.toFixed(2)}</span>
        </div>
        <div class="metrics two-col">
          <div class="metric">
            <span class="metric-label">Heading</span>
            <strong
              >{(($telemetry.heading * 180) / Math.PI).toFixed(1)} deg</strong
            >
          </div>
          <div class="metric">
            <span class="metric-label">EKF</span>
            <strong>{$telemetry.ekf_health}</strong>
          </div>
        </div>
      </div>
    </section>

    <section class="panel">
      <div class="section-title">Akku</div>
      <div class="metrics two-col">
        <div class="metric">
          <span class="metric-label">Spannung</span>
          <strong>{$telemetry.battery_v.toFixed(1)} V</strong>
          <div class="battery-bar" aria-label={`Ladestand ${$batteryPercent}%`}>
            <span
              class="battery-fill"
              style={`width:${Math.max(0, Math.min(100, $batteryPercent))}%`}
            ></span>
          </div>
        </div>
        <div class="metric">
          <span class="metric-label">Dock</span>
          <strong>{$telemetry.charge_v.toFixed(1)} V</strong>
        </div>
      </div>
      <div class="metrics single-row">
        <div class="metric">
          <span class="metric-label">Ladestrom</span>
          <strong>{$telemetry.charge_a.toFixed(2)} A</strong>
        </div>
      </div>
    </section>

    <section class="panel">
      <div class="section-title">Sensoren</div>
      <div class="sensor-grid">
        <div class="sensor">
          <span>Bumper L</span>
          <span class={`sensor-state ${sensorStateClass($telemetry.bumper_l)}`}>
            <span class="sensor-dot"></span>
          </span>
        </div>
        <div class="sensor">
          <span>Bumper R</span>
          <span class={`sensor-state ${sensorStateClass($telemetry.bumper_r)}`}>
            <span class="sensor-dot"></span>
          </span>
        </div>
        <div class="sensor">
          <span>Lift</span>
          <span class={`sensor-state ${sensorStateClass($telemetry.lift)}`}>
            <span class="sensor-dot"></span>
          </span>
        </div>
        <div class="sensor">
          <span>Motor</span>
          <span
            class={`sensor-state ${sensorStateClass($telemetry.motor_err)}`}
          >
            <span class="sensor-dot"></span>
          </span>
        </div>
      </div>
    </section>

    <RobotControls />
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
    gap: 0.4rem;
    padding: 0.58rem 0.68rem;
    border-bottom: 1px solid #0f1829;
  }

  .panel-head,
  .metric,
  .sensor {
    display: grid;
    gap: 0.14rem;
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

  .muted,
  .sensor span {
    color: #94a3b8;
    font-size: 0.64rem;
  }

  .status-pos {
    color: #64748b;
    font-family: "Courier New", monospace;
    font-size: 0.6rem;
  }

  .gps-card {
    display: grid;
    gap: 0.42rem;
    padding: 0.52rem;
    border-radius: 0.5rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .gps-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.4rem;
  }

  .gps-fix {
    color: #4ade80;
    font-size: 0.74rem;
  }

  .gps-sol {
    color: #64748b;
    font-size: 0.58rem;
    font-family: "Courier New", monospace;
    background: #0a1020;
    border-radius: 0.32rem;
    padding: 0.12rem 0.32rem;
  }

  .metrics.two-col,
  .sensor-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 0.36rem;
  }

  .metrics.single-row {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.46rem;
  }

  .metric,
  .sensor {
    padding: 0.42rem 0.5rem;
    border-radius: 0.45rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
  }

  .sensor {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.5rem;
  }

  .sensor-state {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 0.75rem;
    flex-shrink: 0;
    color: #64748b;
  }

  .sensor-state.hot {
    color: #ef4444;
  }

  .sensor-state.idle {
    color: #22c55e;
  }

  .sensor-dot {
    width: 0.44rem;
    height: 0.44rem;
    border-radius: 999px;
    background: currentColor;
    box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.04);
  }

  .battery-bar {
    margin-top: 0.24rem;
    width: 100%;
    height: 0.35rem;
    border-radius: 999px;
    background: #08101c;
    border: 1px solid #16253d;
    overflow: hidden;
  }

  .battery-fill {
    display: block;
    height: 100%;
    border-radius: inherit;
    background: linear-gradient(90deg, #1d4ed8 0%, #22c55e 100%);
  }

  @media (max-width: 980px) {
    .sidebar {
      position: relative;
      width: auto;
      top: auto;
      right: auto;
      bottom: auto;
      margin-top: 0.75rem;
    }

    .sidebar.collapsed {
      width: auto;
    }

    .sr-side {
      margin-left: 0;
      height: auto;
      box-shadow: none;
    }

    .collapse-btn,
    .collapse-btn.collapsed {
      left: auto;
      right: 0.75rem;
      top: -0.65rem;
      transform: none;
      border: 1px solid #1e3a5f;
      border-radius: 6px;
    }

    .metrics.two-col,
    .sensor-grid {
      grid-template-columns: 1fr 1fr;
    }
  }
</style>
