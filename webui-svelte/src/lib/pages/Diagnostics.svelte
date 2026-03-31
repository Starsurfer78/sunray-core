<script lang="ts">
  import PageLayout from "../components/PageLayout.svelte";
  import SensorPanel from "../components/Diagnostics/SensorPanel.svelte";
  import ImuPanel from "../components/Diagnostics/ImuPanel.svelte";
  import MotorTestPanel from "../components/Diagnostics/MotorTestPanel.svelte";
  import TickCalibration from "../components/Diagnostics/TickCalibration.svelte";
  import DirectionValidation from "../components/Diagnostics/DirectionValidation.svelte";
  import TelemetryPanel from "../components/TelemetryPanel.svelte";
  import { diagnostics } from "../stores/diagnostics";
  import { telemetry } from "../stores/telemetry";
  import { getRecoveryNotice, humanizeReason } from "../utils/robotUi";

  let sidebarCollapsed = false;

  $: recoveryNotice = getRecoveryNotice($telemetry);
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="diagnostics-body">
    <TelemetryPanel />

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
      <p>Rohdaten, Sensorstatus und Servicefunktionen gebuendelt an einem Ort.</p>
    </div>

    <div class={`state-card ${recoveryNotice.tone}`}>
      <span class="label">Betriebszustand</span>
      <strong>{recoveryNotice.title}</strong>
      <span>{recoveryNotice.detail}</span>
      <span class="action">{recoveryNotice.action}</span>
    </div>

    <div class="result-card">
      <span class="label">Live-Grund</span>
      <strong>{humanizeReason($telemetry.event_reason)}</strong>
      <span class="muted">
        Resume {$telemetry.resume_target || "—"} · Fehler {$telemetry.error_code || "—"}
      </span>
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
    display: grid;
    gap: 0.35rem;
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

  p {
    margin: 0;
    color: #94a3b8;
    font-size: 0.82rem;
    line-height: 1.45;
  }

  .state-card,
  .result-card {
    margin: 1rem;
    display: grid;
    gap: 0.25rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .state-card.success {
    border-color: #166534;
  }

  .state-card.warning {
    border-color: #92400e;
  }

  .state-card.error {
    border-color: #7f1d1d;
  }

  .state-card.info {
    border-color: #1d4ed8;
  }

  .action {
    color: #dbeafe;
    font-size: 0.82rem;
    line-height: 1.45;
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
