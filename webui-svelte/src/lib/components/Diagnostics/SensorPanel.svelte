<script lang="ts">
  import { telemetry } from '../../stores/telemetry'

  const sensors = [
    { label: 'Bumper links', value: 'bumper_l' },
    { label: 'Bumper rechts', value: 'bumper_r' },
    { label: 'Lift', value: 'lift' },
    { label: 'Motorfehler', value: 'motor_err' },
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
      <article class="card">
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
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header, .grid {
    display: grid;
    gap: 0.8rem;
  }

  h2, p {
    margin: 0;
  }

  p {
    color: #9db3ab;
  }

  .grid {
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  }

  .card {
    display: grid;
    gap: 0.35rem;
    padding: 0.9rem;
    border-radius: 0.9rem;
    background: rgba(24, 38, 34, 0.8);
    border: 1px solid rgba(152, 187, 170, 0.12);
  }

  strong {
    color: #c9d7d1;
  }

  .active {
    color: #9be38e;
  }
</style>
