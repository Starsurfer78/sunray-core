<script lang="ts">
  import { telemetry } from '../../stores/telemetry'

  const sensors = [
    { label: 'Bumper links', value: 'bumper_l' },
    { label: 'Bumper rechts', value: 'bumper_r' },
    { label: 'Stop-Button', value: 'stop_button' },
    { label: 'Lift', value: 'lift' },
    { label: 'Mähmotorfehler', value: 'motor_err' },
    { label: 'Diag aktiv', value: 'diag_active' },
  ] as const
</script>

<section class="panel">
  <header>
    <h2>Sensoren live</h2>
    <p>Direkte Rueckmeldung fuer Bumper, Lift, Motorstatus und Diagnosebetrieb.</p>
  </header>

  <div class="grid">
    {#each sensors as sensor}
      <article class="card" class:active-card={$telemetry[sensor.value]}>
        <span>{sensor.label}</span>
        <strong class:active={$telemetry[sensor.value]}>
          {$telemetry[sensor.value] ? 'Aktiv' : 'Aus'}
        </strong>
      </article>
    {/each}
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

  header, .grid {
    display: grid;
    gap: 0.8rem;
  }

  h2, p {
    margin: 0;
  }

  p { color: #64748b; font-size: 0.84rem; }

  .grid {
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  }

  .card {
    display: grid;
    gap: 0.35rem;
    padding: 0.9rem;
    border-radius: 0.7rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
    transition: border-color 120ms ease, background 120ms ease, transform 120ms ease;
  }

  strong { color: #94a3b8; }

  .active { color: #4ade80; }

  .active-card {
    border-color: rgba(74, 222, 128, 0.72);
    background: linear-gradient(180deg, rgba(12, 48, 28, 0.96), rgba(8, 28, 18, 0.96));
    transform: translateY(-1px);
  }
</style>
