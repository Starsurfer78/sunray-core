<script lang="ts">
  import RobotControls from '../components/RobotControls.svelte'
  import TelemetryPanel from '../components/TelemetryPanel.svelte'
  import { telemetry } from '../stores/telemetry'
</script>

<main class="page">
  <section class="head">
    <div>
      <span class="eyebrow">Dashboard</span>
      <h1>Betriebsueberblick</h1>
    </div>
    <div class="state-card">
      <span class="label">Aktiver Zustand</span>
      <strong>{$telemetry.op}</strong>
      <span class="muted">Phase {$telemetry.state_phase || 'idle'}</span>
    </div>
  </section>

  <section class="content">
    <TelemetryPanel />
    <RobotControls />
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1rem;
  }

  .head {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 260px;
    gap: 1rem;
    align-items: start;
  }

  .eyebrow {
    display: inline-block;
    margin-bottom: 0.35rem;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }

  h1 { margin: 0; }

  h1 {
    font-size: clamp(1.4rem, 2vw, 1.9rem);
    line-height: 1.05;
  }

  .state-card {
    display: grid;
    gap: 0.25rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .state-card strong {
    font-size: 1.3rem;
    color: #93c5fd;
  }

  .label {
    color: #7a8da8;
    font-size: 0.76rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .muted {
    color: #94a3b8;
    font-size: 0.82rem;
  }

  .content {
    display: grid;
    grid-template-columns: minmax(0, 1.5fr) minmax(280px, 0.8fr);
    gap: 1rem;
    align-items: start;
  }

  @media (max-width: 900px) {
    .head,
    .content {
      grid-template-columns: 1fr;
    }
  }
</style>
