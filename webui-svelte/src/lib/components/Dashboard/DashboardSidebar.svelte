<script lang="ts">
  import RobotControls from '../RobotControls.svelte'
  import { gpsQuality, batteryPercent, telemetry } from '../../stores/telemetry'

  const boolLabel = (value: boolean) => value ? 'Aktiv' : 'Aus'
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
      </div>
      <div class="metric">
        <span class="metric-label">Ladung</span>
        <strong>{$batteryPercent}%</strong>
      </div>
      <div class="metric">
        <span class="metric-label">Dock</span>
        <strong>{$telemetry.charge_v.toFixed(1)} V</strong>
      </div>
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
        <strong class:ok={$telemetry.bumper_l}>{boolLabel($telemetry.bumper_l)}</strong>
      </div>
      <div class="sensor">
        <span>Bumper R</span>
        <strong class:ok={$telemetry.bumper_r}>{boolLabel($telemetry.bumper_r)}</strong>
      </div>
      <div class="sensor">
        <span>Lift</span>
        <strong class:ok={$telemetry.lift}>{boolLabel($telemetry.lift)}</strong>
      </div>
      <div class="sensor">
        <span>Motor</span>
        <strong class:ok={$telemetry.motor_err}>{boolLabel($telemetry.motor_err)}</strong>
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
    gap: 0.75rem;
    padding: 1rem;
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
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .section-title {
    margin-bottom: 0.05rem;
  }

  strong {
    color: #dbeafe;
    font-size: 1rem;
  }

  .panel-head strong {
    font-size: 1.6rem;
    color: #60a5fa;
  }

  .muted,
  .sensor span {
    color: #94a3b8;
    font-size: 0.8rem;
  }

  .status-pos {
    color: #64748b;
    font-family: 'Courier New', monospace;
    font-size: 0.75rem;
  }

  .gps-card {
    display: grid;
    gap: 0.75rem;
    padding: 0.75rem;
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
    gap: 0.65rem;
  }

  .metric,
  .sensor {
    padding: 0.75rem;
    border-radius: 0.7rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
  }

  .sensor strong {
    color: #93c5fd;
    font-size: 0.9rem;
  }

  .sensor strong.ok {
    color: #34d399;
  }

  @media (max-width: 980px) {
    .metrics.two-col,
    .sensor-grid {
      grid-template-columns: 1fr 1fr;
    }
  }
</style>
