<script lang="ts">
  import PageLayout from "../components/PageLayout.svelte";
  import SensorPanel from "../components/Diagnostics/SensorPanel.svelte";
  import ImuPanel from "../components/Diagnostics/ImuPanel.svelte";
  import MotorTestPanel from "../components/Diagnostics/MotorTestPanel.svelte";
  import TickCalibration from "../components/Diagnostics/TickCalibration.svelte";
  import DirectionValidation from "../components/Diagnostics/DirectionValidation.svelte";
  import { diagnostics } from "../stores/diagnostics";

  let sidebarCollapsed = false;
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="diagnostics-body">
    <!-- Sensoren Live als erstes, prominentes Panel -->
    <div class="sensor-highlight">
      <SensorPanel />
    </div>

    <!-- Rest der Panels in einem Grid darunter -->
    <div class="diagnostics-grid">
      <ImuPanel />
      <MotorTestPanel />
      <TickCalibration />
      <DirectionValidation />
    </div>
  </div>

  <svelte:fragment slot="sidebar">
    <div class="sidebar-header">
      <span class="eyebrow">Diagnose</span>
      <h1>Hardware und Kalibrierung</h1>
    </div>

    <div class="result-card">
      <span class="label">Letzte Aktion</span>
      <strong>{$diagnostics.activeAction ?? "bereit"}</strong>
      {#if $diagnostics.error}
        <span class="error">{$diagnostics.error}</span>
      {:else if $diagnostics.lastResult}
        <span class="ok">
          {$diagnostics.lastResult.message ??
            ($diagnostics.lastResult.ok ? "erfolgreich" : "fehlgeschlagen")}
        </span>
      {:else}
        <span class="muted">Noch kein Ergebnis</span>
      {/if}
    </div>
  </svelte:fragment>
</PageLayout>

<style>
  .diagnostics-body {
    height: 100%;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding: 1rem;
  }

  .sensor-highlight {
    flex-shrink: 0;
  }

  .diagnostics-grid {
    flex: 1;
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1rem;
    align-items: start;
  }

  @media (max-width: 900px) {
    .diagnostics-grid {
      grid-template-columns: 1fr;
    }
  }

  .sidebar-header {
    padding: 1rem;
    border-bottom: 1px solid #1e3a5f;
  }

  .eyebrow {
    display: inline-block;
    margin-bottom: 0.35rem;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }

  h1 {
    margin: 0;
    font-size: 1.55rem;
    line-height: 1.05;
  }

  .result-card {
    margin: 1rem;
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
</style>
