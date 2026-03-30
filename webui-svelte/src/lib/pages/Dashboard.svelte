<script lang="ts">
  import RobotControls from '../components/RobotControls.svelte'
  import TelemetryPanel from '../components/TelemetryPanel.svelte'
  import { telemetry } from '../stores/telemetry'
</script>

<main class="page">
  <section class="title-row">
    <div>
      <span class="eyebrow">Dashboard</span>
      <h1>Betriebsueberblick</h1>
    </div>
  </section>

  <section class="layout">
    <div class="main-column">
      <TelemetryPanel />
    </div>

    <aside class="sidebar">
      <div class="state-card">
        <span class="label">Aktiver Zustand</span>
        <strong>{$telemetry.op}</strong>
        <span class="muted">Phase {$telemetry.state_phase || 'idle'}</span>
      </div>
      <RobotControls />
    </aside>
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1rem;
  }

  .title-row { display: block; }

  .eyebrow {
    display: inline-block;
    margin-bottom: 0.35rem;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }

  h1 { margin: 0; }

  h1 { font-size: 1.7rem; line-height: 1.05; }

  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 320px;
    gap: 1rem;
    align-items: start;
  }

  .main-column,
  .sidebar {
    min-width: 0;
  }

  .sidebar {
    display: grid;
    gap: 1rem;
    align-content: start;
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

  @media (max-width: 900px) {
    .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
