<script lang="ts">
  import SensorPanel from '../components/Diagnostics/SensorPanel.svelte'
  import ImuPanel from '../components/Diagnostics/ImuPanel.svelte'
  import MotorTestPanel from '../components/Diagnostics/MotorTestPanel.svelte'
  import TickCalibration from '../components/Diagnostics/TickCalibration.svelte'
  import DirectionValidation from '../components/Diagnostics/DirectionValidation.svelte'
  import { diagnostics } from '../stores/diagnostics'
</script>

<main class="page">
  <section class="hero">
    <div>
      <span class="eyebrow">Diagnose</span>
      <h1>Hardware und Kalibrierung</h1>
      <p>
        Erste Diagnoseansicht fuer Sensoren, IMU und Motor-Tests. Tick-Messung
        und Richtungsvalidierung bauen darauf auf.
      </p>
    </div>

    <div class="result-card">
      <span class="label">Letzte Aktion</span>
      <strong>{$diagnostics.activeAction ?? 'bereit'}</strong>
      {#if $diagnostics.error}
        <span class="error">{$diagnostics.error}</span>
      {:else if $diagnostics.lastResult}
        <span class="ok">
          {$diagnostics.lastResult.message ?? ($diagnostics.lastResult.ok ? 'erfolgreich' : 'fehlgeschlagen')}
        </span>
      {:else}
        <span class="muted">Noch kein Ergebnis</span>
      {/if}
    </div>
  </section>

  <section class="content">
    <SensorPanel />
    <ImuPanel />
    <MotorTestPanel />
    <TickCalibration />
    <DirectionValidation />
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1.2rem;
  }

  .hero {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 300px;
    gap: 1rem;
  }

  .eyebrow {
    display: inline-block;
    margin-bottom: 0.6rem;
    color: #f0c67f;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.8rem;
  }

  h1, p {
    margin: 0;
  }

  h1 {
    font-size: clamp(2rem, 4vw, 3.2rem);
    line-height: 1;
    margin-bottom: 0.6rem;
  }

  p {
    color: #a5bab4;
  }

  .result-card {
    display: grid;
    gap: 0.35rem;
    padding: 1.2rem;
    border-radius: 1rem;
    background: linear-gradient(160deg, rgba(170, 133, 82, 0.3), rgba(23, 37, 33, 0.9));
    border: 1px solid rgba(221, 191, 140, 0.2);
  }

  .label {
    color: #d0d6b2;
  }

  .muted {
    color: #93a59e;
  }

  .ok {
    color: #a8e2a1;
  }

  .error {
    color: #f1aaaa;
  }

  .content {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1rem;
    align-items: start;
  }

  :global(.content > :nth-child(3)),
  :global(.content > :nth-child(4)),
  :global(.content > :nth-child(5)) {
    grid-column: span 2;
  }

  @media (max-width: 900px) {
    .hero,
    .content {
      grid-template-columns: 1fr;
    }

    :global(.content > :nth-child(3)),
    :global(.content > :nth-child(4)),
    :global(.content > :nth-child(5)) {
      grid-column: span 1;
    }
  }
</style>
