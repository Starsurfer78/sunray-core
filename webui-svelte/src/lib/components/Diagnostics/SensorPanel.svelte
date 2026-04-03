<script lang="ts">
  import { telemetry } from '../../stores/telemetry'

  const sensors = [
    { label: 'Bumper links', value: 'bumper_l' },
    { label: 'Bumper rechts', value: 'bumper_r' },
    { label: 'Stop-Button', value: 'stop_button' },
    { label: 'Lift', value: 'lift' },
    { label: 'Mähmotorfehler', value: 'motor_err' },
    { label: 'Fault-Pin', value: 'mow_fault_pin' },
    { label: 'Überlast aktiv', value: 'mow_overload' },
    { label: 'Permanent Fault', value: 'mow_permanent_fault' },
    { label: 'OV-Check', value: 'mow_ov_check' },
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
    gap: 0.65rem;
    padding: 0.75rem 0.85rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  header {
    display: grid;
    gap: 0.2rem;
  }

  h2, p { margin: 0; }

  h2 { font-size: 0.92rem; color: #e2e8f0; }

  p { color: #64748b; font-size: 0.78rem; }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
    gap: 0.45rem;
  }

  .card {
    display: grid;
    gap: 0.2rem;
    padding: 0.5rem 0.65rem;
    border-radius: 0.55rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
    transition: border-color 120ms ease, background 120ms ease;
  }

  .card span { font-size: 0.78rem; color: #64748b; }

  strong { font-size: 0.82rem; color: #94a3b8; }

  .active { color: #4ade80; }

  .active-card {
    border-color: rgba(74, 222, 128, 0.6);
    background: linear-gradient(180deg, rgba(12, 48, 28, 0.96), rgba(8, 28, 18, 0.96));
  }
</style>
