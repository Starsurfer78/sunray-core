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
    <div class="hero">
      <div>
        <span class="eyebrow">Diagnose</span>
        <h1>Hardware und Kalibrierung</h1>
        <p>Rohdaten, Sensorstatus und Servicefunktionen gebuendelt an einem Ort.</p>
      </div>
    </div>

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
      <span class="sb-eyebrow">Diagnose</span>
      <h2>Hardware und Kalibrierung</h2>
      <p>Rohdaten, Sensorstatus und Servicefunktionen.</p>
    </div>

    <div class="sr-sec">
      <div class="sr-lbl">Betriebszustand</div>
      <div class="sr-stbig">{recoveryNotice.title}</div>
      <div class="sr-detail">{recoveryNotice.detail}</div>
      {#if recoveryNotice.action}
        <div class="sr-action">{recoveryNotice.action}</div>
      {/if}
    </div>

    <div class="sr-sec">
      <div class="sr-lbl">Live-Grund</div>
      <div class="sr-stbig">{humanizeReason($telemetry.event_reason)}</div>
      <div class="sr-detail">
        Resume {$telemetry.resume_target || "—"} · Fehler {$telemetry.error_code || "—"}
      </div>
    </div>

    <div class="sr-sec">
      <div class="sr-lbl">Letzte Aktion</div>
      <div class="sr-stbig">{$diagnostics.activeAction ?? "bereit"}</div>
      {#if $diagnostics.error}
        <div class="sr-err">{$diagnostics.error}</div>
      {:else if $diagnostics.lastResult}
        <div class="sr-ok">
          {$diagnostics.lastResult.message ??
            ($diagnostics.lastResult.ok ? "erfolgreich" : "fehlgeschlagen")}
        </div>
      {:else}
        <div class="sr-muted">Noch kein Ergebnis</div>
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
    background:
      radial-gradient(circle at top right, rgba(30, 58, 95, 0.2), transparent 40%),
      linear-gradient(180deg, rgba(7, 13, 24, 0.98), rgba(10, 15, 26, 0.98));
  }

  .hero {
    padding: 1rem 1.1rem;
    border-radius: 1rem;
    background: rgba(15, 24, 41, 0.95);
    border: 1px solid #1e3a5f;
    flex-shrink: 0;
  }

  .eyebrow {
    display: inline-block;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
    margin-bottom: 0.35rem;
  }

  h1 {
    margin: 0;
    font-size: 1.45rem;
    line-height: 1.1;
    color: #f8fafc;
  }

  .hero p {
    margin: 0.35rem 0 0;
    color: #94a3b8;
    font-size: 0.84rem;
    line-height: 1.45;
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
    border-bottom: 1px solid #0f1829;
    display: grid;
    gap: 0.3rem;
  }

  .sb-eyebrow {
    display: inline-block;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }

  h2 { margin: 0; font-size: 1.1rem; color: #e2e8f0; }

  .sidebar-header p {
    margin: 0;
    color: #94a3b8;
    font-size: 0.8rem;
    line-height: 1.4;
  }

  .sr-sec {
    padding: 10px 12px;
    border-bottom: 1px solid #0f1829;
    display: grid;
    gap: 0.35rem;
    flex-shrink: 0;
  }

  .sr-lbl {
    font-size: 10px;
    font-weight: 500;
    color: #475569;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 2px;
  }

  .sr-stbig { font-size: 14px; font-weight: 500; color: #60a5fa; }
  .sr-detail { font-size: 11px; color: #64748b; line-height: 1.4; }
  .sr-action { font-size: 11px; color: #dbeafe; }
  .sr-ok     { font-size: 11px; color: #86efac; }
  .sr-err    { font-size: 11px; color: #fca5a5; }
  .sr-muted  { font-size: 11px; color: #64748b; }
</style>
