<script lang="ts">
  import StatusBar from '../components/StatusBar.svelte'
  import RobotControls from '../components/RobotControls.svelte'
  import TelemetryPanel from '../components/TelemetryPanel.svelte'
  import { telemetry } from '../stores/telemetry'
</script>

<main class="page">
  <section class="hero">
    <div>
      <span class="eyebrow">Sunray Alfred</span>
      <h1>Dashboard MVP</h1>
      <p>
        Erster robuster Ueberblick fuer Verbindung, Live-Telemetrie und
        Basis-Steuerung. Diagnose, Karte und Mission folgen als naechste
        Bausteine.
      </p>
    </div>
    <div class="hero-card">
      <span class="label">Aktiver Zustand</span>
      <strong>{$telemetry.op}</strong>
      <span class="muted">Phase {$telemetry.state_phase || 'idle'}</span>
    </div>
  </section>

  <StatusBar />

  <section class="content">
    <TelemetryPanel />
    <RobotControls />
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1.2rem;
    max-width: 1200px;
    margin: 0 auto;
    padding: 1.4rem;
  }

  .hero {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 280px;
    gap: 1rem;
    align-items: stretch;
  }

  .eyebrow {
    display: inline-block;
    margin-bottom: 0.6rem;
    color: #a4cc8c;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.8rem;
  }

  h1,
  p {
    margin: 0;
  }

  h1 {
    font-size: clamp(2rem, 4vw, 3.5rem);
    line-height: 1;
    margin-bottom: 0.6rem;
  }

  p {
    max-width: 56rem;
    color: #a5bab4;
  }

  .hero-card {
    display: grid;
    gap: 0.35rem;
    padding: 1.2rem;
    border-radius: 1rem;
    background:
      linear-gradient(160deg, rgba(116, 154, 93, 0.26), rgba(23, 37, 33, 0.9));
    border: 1px solid rgba(188, 221, 153, 0.2);
  }

  .hero-card strong {
    font-size: 2rem;
  }

  .label {
    color: #b8cdbf;
  }

  .muted {
    color: #89a099;
  }

  .content {
    display: grid;
    grid-template-columns: minmax(0, 1.5fr) minmax(280px, 0.8fr);
    gap: 1rem;
    align-items: start;
  }

  @media (max-width: 900px) {
    .hero,
    .content {
      grid-template-columns: 1fr;
    }
  }
</style>
