<script lang="ts">
  import { gpsQuality, telemetry } from '../stores/telemetry'

  const RAD_TO_DEG = 180 / Math.PI
</script>

<section class="panel">
  <header>
    <h2>Live-Telemetrie</h2>
    <p>Kompakte Betriebsdaten für Status, Sensoren und Lage.</p>
  </header>

  <div class="grid">
    <article class="card wide">
      <span class="label">Position lokal</span>
      <strong>{$telemetry.x.toFixed(2)} m / {$telemetry.y.toFixed(2)} m</strong>
      <span class="muted">
        Pose {(($telemetry.heading * RAD_TO_DEG)).toFixed(1)} deg · {$telemetry.ekf_health}
      </span>
    </article>

    <article class="card">
      <span class="label">GPS</span>
      <strong>{$gpsQuality}</strong>
      <span class="muted">Acc {$telemetry.gps_acc.toFixed(2)} m</span>
    </article>

    <article class="card">
      <span class="label">IMU</span>
      <strong>H {$telemetry.imu_h.toFixed(1)} deg</strong>
      <span class="muted">R {$telemetry.imu_r.toFixed(1)} deg / P {$telemetry.imu_p.toFixed(1)} deg</span>
    </article>

    <article class="card">
      <span class="label">Diag</span>
      <strong>{$telemetry.diag_active ? 'Laeuft' : 'Aus'}</strong>
      <span class="muted">Ticks {$telemetry.diag_ticks}</span>
    </article>
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1rem;
    border-radius: 0.9rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  header {
    display: grid;
    gap: 0.25rem;
  }

  h2,
  p {
    margin: 0;
  }

  p {
    color: #64748b;
    font-size: 0.84rem;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 0.85rem;
  }

  .card {
    display: grid;
    gap: 0.25rem;
    padding: 0.9rem;
    border-radius: 0.7rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
  }

  .wide {
    grid-column: span 2;
  }

  .label {
    color: #64748b;
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .muted {
    color: #64748b;
    font-size: 0.78rem;
  }

  strong {
    font-size: 1rem;
    color: #60a5fa;
  }

  @media (max-width: 720px) {
    .wide {
      grid-column: span 1;
    }
  }
</style>
