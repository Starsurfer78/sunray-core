<script lang="ts">
  import SensorPanel from '../components/Diagnostics/SensorPanel.svelte'
  import ImuPanel from '../components/Diagnostics/ImuPanel.svelte'
  import MotorTestPanel from '../components/Diagnostics/MotorTestPanel.svelte'
  import TickCalibration from '../components/Diagnostics/TickCalibration.svelte'
  import DirectionValidation from '../components/Diagnostics/DirectionValidation.svelte'
  import { diagnostics } from '../stores/diagnostics'
</script>

<main class="page">
  <section class="head">
    <div>
      <span class="eyebrow">Diagnose</span>
      <h1>Hardware und Kalibrierung</h1>
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
    gap: 1rem;
    padding: 1rem;
  }

  .head {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 300px;
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

  h1 { font-size: 1.55rem; line-height: 1.05; }

  .result-card {
    display: grid;
    gap: 0.25rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
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

  .ok {
    color: #a8e2a1;
    font-size: 0.82rem;
  }

  .error {
    color: #f1aaaa;
    font-size: 0.82rem;
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
    .head,
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
