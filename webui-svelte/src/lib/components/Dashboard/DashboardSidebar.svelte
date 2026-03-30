<script lang="ts">
  import RobotControls from '../RobotControls.svelte'
  import { gpsQuality, batteryPercent, telemetry } from '../../stores/telemetry'

  const sensorStateClass = (value: boolean) => value ? 'hot' : 'idle'
</script>

<aside class="sidebar">
  <section class="panel">
    <div class="panel-head">
      <span class="label">Status</span>
      <strong>{$telemetry.op}</strong>
      <span class="muted">Phase {$telemetry.state_phase || 'idle'}</span>
      <span class="status-pos">X {$telemetry.x.toFixed(2)} m · Y {$telemetry.y.toFixed(2)} m</span>
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
          <strong>{(($telemetry.heading * 180) / Math.PI).toFixed(1)} deg</strong>
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
          <span class="battery-fill" style={`width:${Math.max(0, Math.min(100, $batteryPercent))}%`}></span>
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
        <strong class={sensorStateClass($telemetry.bumper_l)}>
          <span class="sensor-dot"></span>
        </strong>
      </div>
      <div class="sensor">
        <span>Bumper R</span>
        <strong class={sensorStateClass($telemetry.bumper_r)}>
          <span class="sensor-dot"></span>
        </strong>
      </div>
      <div class="sensor">
        <span>Lift</span>
        <strong class={sensorStateClass($telemetry.lift)}>
          <span class="sensor-dot"></span>
        </strong>
      </div>
      <div class="sensor">
        <span>Motor</span>
        <strong class={sensorStateClass($telemetry.motor_err)}>
          <span class="sensor-dot"></span>
        </strong>
      </div>
    </div>
  </section>

  <RobotControls />
</aside>

<style>
  .sidebar {
    display: grid;
    gap: 0;
    align-content: start;
    background: #0a1020;
    border-left: 1px solid #1e3a5f;
  }

  .panel {
    display: grid;
    gap: 0.65rem;
    padding: 0.85rem 0.95rem;
    border-bottom: 1px solid #0f1829;
  }

  .panel-head,
  .metric,
  .sensor {
    display: grid;
    gap: 0.18rem;
  }

  .label,
  .section-title,
  .metric-label {
    color: #7a8da8;
    font-size: 0.68rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .section-title {
    margin-bottom: 0.05rem;
  }

  strong {
    color: #dbeafe;
    font-size: 0.95rem;
  }

  .panel-head strong {
    font-size: 1.35rem;
    color: #60a5fa;
  }

  .muted,
  .sensor span {
    color: #94a3b8;
    font-size: 0.74rem;
  }

  .status-pos {
    color: #64748b;
    font-family: 'Courier New', monospace;
    font-size: 0.7rem;
  }

  .gps-card {
    display: grid;
    gap: 0.65rem;
    padding: 0.68rem;
    border-radius: 0.7rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .gps-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.5rem;
  }

  .gps-fix {
    color: #4ade80;
    font-size: 0.95rem;
  }

  .gps-sol {
    color: #64748b;
    font-size: 0.72rem;
    font-family: 'Courier New', monospace;
    background: #0a1020;
    border-radius: 0.35rem;
    padding: 0.18rem 0.42rem;
  }

  .metrics.two-col,
  .sensor-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 0.55rem;
  }

  .metrics.single-row {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.55rem;
  }

  .metric,
  .sensor {
    padding: 0.62rem 0.68rem;
    border-radius: 0.62rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
  }

  .sensor strong {
    display: inline-flex;
    align-items: center;
    justify-content: flex-end;
    color: #64748b;
    font-size: 0.82rem;
  }

  .sensor strong.hot {
    color: #ef4444;
  }

  .sensor strong.idle {
    color: #22c55e;
  }

  .sensor-dot {
    width: 0.58rem;
    height: 0.58rem;
    border-radius: 999px;
    background: currentColor;
    box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.04);
  }

  .battery-bar {
    margin-top: 0.28rem;
    width: 100%;
    height: 0.38rem;
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
    .metrics.two-col,
    .sensor-grid {
      grid-template-columns: 1fr 1fr;
    }
  }
</style>
