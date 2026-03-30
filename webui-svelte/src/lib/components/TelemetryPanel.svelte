<script lang="ts">
  import { gpsQuality, telemetry } from '../stores/telemetry'

  const cards = [
    { label: 'Bumper links', key: 'bumper_l' },
    { label: 'Bumper rechts', key: 'bumper_r' },
    { label: 'Lift', key: 'lift' },
    { label: 'Motorfehler', key: 'motor_err' },
  ] as const
</script>

<section class="panel">
  <header>
    <h2>Live-Telemetrie</h2>
    <p>Basisdaten fuer den ersten lauffaehigen Dashboard-Wurf.</p>
  </header>

  <div class="grid">
    <article class="card wide">
      <span class="label">Position lokal</span>
      <strong>{$telemetry.x.toFixed(2)} m / {$telemetry.y.toFixed(2)} m</strong>
      <span class="muted">Heading {$telemetry.heading.toFixed(2)} rad</span>
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

    {#each cards as card}
      <article class="card">
        <span class="label">{card.label}</span>
        <strong>{$telemetry[card.key] ? 'Aktiv' : 'Aus'}</strong>
      </article>
    {/each}
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

  header {
    display: grid;
    gap: 0.25rem;
  }

  h2,
  p {
    margin: 0;
  }

  p {
    color: #9db3ab;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 0.85rem;
  }

  .card {
    display: grid;
    gap: 0.25rem;
    padding: 0.95rem;
    border-radius: 0.9rem;
    background: rgba(24, 38, 34, 0.8);
    border: 1px solid rgba(152, 187, 170, 0.12);
  }

  .wide {
    grid-column: span 2;
  }

  .label {
    color: #9db3ab;
    font-size: 0.9rem;
  }

  .muted {
    color: #7f9890;
    font-size: 0.9rem;
  }

  strong {
    font-size: 1.05rem;
  }

  @media (max-width: 720px) {
    .wide {
      grid-column: span 1;
    }
  }
</style>
