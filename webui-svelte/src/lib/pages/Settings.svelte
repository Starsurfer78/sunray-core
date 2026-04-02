<script lang="ts">
  import OtaUpdate from "../components/OtaUpdate.svelte";
  import { stmProbe, type StmProbeResponse } from "../api/rest";
  import SensorPanel from "../components/Diagnostics/SensorPanel.svelte";
  import ImuPanel from "../components/Diagnostics/ImuPanel.svelte";
  import MotorTestPanel from "../components/Diagnostics/MotorTestPanel.svelte";
  import TickCalibration from "../components/Diagnostics/TickCalibration.svelte";
  import DirectionValidation from "../components/Diagnostics/DirectionValidation.svelte";
  import { telemetry } from "../stores/telemetry";
  import { diagnostics } from "../stores/diagnostics";
  import { getRecoveryNotice, humanizeReason } from "../utils/robotUi";

  type Section = "general" | "diagnostics";

  let activeSection: Section = "general";
  let stmProbeBusy = false;
  let stmProbeResult: StmProbeResponse | null = null;
  let stmProbeError = "";

  $: recoveryNotice = getRecoveryNotice($telemetry);

  async function runStmProbe() {
    stmProbeBusy = true;
    stmProbeError = "";
    stmProbeResult = null;
    try {
      stmProbeResult = await stmProbe();
    } catch (error) {
      stmProbeError = error instanceof Error ? error.message : "STM-Probe fehlgeschlagen";
    } finally {
      stmProbeBusy = false;
    }
  }
</script>

<section class="settings-shell">
  <aside class="settings-nav">
    <div class="settings-nav-head">
      <span class="eyebrow">Einstellungen</span>
      <h1>System und Service</h1>
      <p>Selten genutzte Werkzeuge bewusst aus der Hauptnavigation herausgenommen.</p>
    </div>

    <nav class="settings-menu" aria-label="Einstellungsbereiche">
      <button
        type="button"
        class:active={activeSection === "general"}
        on:click={() => (activeSection = "general")}
      >
        <span>Allgemein</span>
        <small>OTA, Runtime, Versionen</small>
      </button>
      <button
        type="button"
        class:active={activeSection === "diagnostics"}
        on:click={() => (activeSection = "diagnostics")}
      >
        <span>Diagnose</span>
        <small>Sensoren, Kalibrierung, Motor-Tests</small>
      </button>
    </nav>
  </aside>

  <div class="settings-content">
    {#if activeSection === "general"}
      <div class="section-head">
        <span class="eyebrow">Allgemein</span>
        <h2>Systemstatus und Update</h2>
        <p>Hier liegen die Dinge, die man nur gelegentlich anfasst.</p>
      </div>

      <div class="info-grid">
        <div class="info-card">
          <span class="label">Runtime</span>
          <strong>{$telemetry.runtime_health || "ok"}</strong>
          <span>Op: {$telemetry.op || "—"}</span>
          <span>Phase: {$telemetry.state_phase || "—"}</span>
          <span>Resume: {$telemetry.resume_target || "—"}</span>
        </div>

        <div class="info-card">
          <span class="label">Versionen</span>
          <strong>{$telemetry.pi_v || "—"}</strong>
          <span>MCU: {$telemetry.mcu_v || "—"}</span>
          <span>GPS: {$telemetry.gps_text || "—"}</span>
          <span>History: {$telemetry.history_backend_ready ? "bereit" : "nicht bereit"}</span>
        </div>

        <div class="info-card">
          <span class="label">Letztes Ereignis</span>
          <strong>{humanizeReason($telemetry.event_reason)}</strong>
          <span>Fehler: {$telemetry.error_code || "—"}</span>
          <span>Zustand: {recoveryNotice.title}</span>
          <span>{recoveryNotice.detail}</span>
        </div>
      </div>

      <div class="panel-card">
        <OtaUpdate />
      </div>

      <div class="panel-card stm-panel">
        <div class="panel-head">
          <span class="eyebrow">STM32 Firmware</span>
          <h3>SWD-Verbindung prüfen</h3>
          <p>Testet nur, ob Alfred aktuell eine flashfähige SWD-Verbindung zum STM32 aufbauen kann.</p>
        </div>

        <div class="stm-actions">
          <button type="button" class="probe-btn" disabled={stmProbeBusy} on:click={runStmProbe}>
            {stmProbeBusy ? "Prüfe SWD ..." : "SWD-Verbindung prüfen"}
          </button>
          <span class="stm-note">Kein Flash, nur `flash_alfred.sh probe`.</span>
        </div>

        {#if stmProbeResult}
          <div class="stm-result ok">
            <strong>Flashfähige Verbindung erkannt</strong>
            <pre>{stmProbeResult.detail}</pre>
          </div>
        {/if}

        {#if stmProbeError}
          <div class="stm-result error">
            <strong>Keine flashfähige Verbindung</strong>
            <pre>{stmProbeError}</pre>
          </div>
        {/if}
      </div>
    {:else}
      <div class="section-head">
        <span class="eyebrow">Diagnose</span>
        <h2>Hardware und Kalibrierung</h2>
        <p>Live-Sensorik und Servicefunktionen gebündelt auf einer ruhigeren Unterseite.</p>
      </div>

      <div class="diag-summary">
        <div class={`state-card ${recoveryNotice.tone}`}>
          <span class="label">Betriebszustand</span>
          <strong>{recoveryNotice.title}</strong>
          <span>{recoveryNotice.detail}</span>
          <span class="action">{recoveryNotice.action}</span>
        </div>

        <div class="state-card neutral">
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
      </div>

      <div class="sensor-highlight">
        <SensorPanel />
      </div>

      <div class="diagnostics-grid">
        <ImuPanel />
        <MotorTestPanel />
        <TickCalibration />
        <DirectionValidation />
      </div>
    {/if}
  </div>
</section>

<style>
  .settings-shell {
    height: 100%;
    display: grid;
    grid-template-columns: 250px minmax(0, 1fr);
    gap: 1rem;
    padding: 1rem;
    box-sizing: border-box;
  }

  .settings-nav {
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding: 1rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.9rem;
    background: #0a1020;
    box-shadow: 0 18px 40px rgba(0, 0, 0, 0.24);
  }

  .settings-nav-head,
  .section-head {
    display: grid;
    gap: 0.35rem;
  }

  .eyebrow {
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }

  h1,
  h2 {
    margin: 0;
    font-size: 1.45rem;
    line-height: 1.05;
    color: #e2e8f0;
  }

  p {
    margin: 0;
    color: #94a3b8;
    font-size: 0.82rem;
    line-height: 1.45;
  }

  .settings-menu {
    display: grid;
    gap: 0.5rem;
  }

  .settings-menu button {
    display: grid;
    gap: 0.18rem;
    padding: 0.8rem 0.9rem;
    border-radius: 0.7rem;
    border: 1px solid #1e3a5f;
    background: #0f1829;
    color: #dbeafe;
    cursor: pointer;
    text-align: left;
  }

  .settings-menu button.active {
    border-color: #60a5fa;
    background: #10213b;
  }

  .settings-menu span {
    font-size: 0.9rem;
    font-weight: 600;
  }

  .settings-menu small {
    color: #7a8da8;
    font-size: 0.72rem;
    line-height: 1.35;
  }

  .settings-content {
    min-width: 0;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding-right: 0.25rem;
  }

  .info-grid,
  .diag-summary {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 1rem;
  }

  .info-card,
  .state-card,
  .panel-card {
    display: grid;
    gap: 0.25rem;
    padding: 1rem;
    border-radius: 0.9rem;
    border: 1px solid #1e3a5f;
    background: #0f1829;
  }

  .panel-card {
    overflow: hidden;
  }

  .stm-panel {
    gap: 0.8rem;
  }

  .panel-head {
    display: grid;
    gap: 0.3rem;
  }

  h3 {
    margin: 0;
    color: #e2e8f0;
    font-size: 1.05rem;
  }

  .stm-actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.6rem;
  }

  .probe-btn {
    padding: 0.55rem 0.85rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #10213b;
    color: #dbeafe;
    cursor: pointer;
    font-weight: 600;
  }

  .probe-btn:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }

  .stm-note {
    color: #7a8da8;
    font-size: 0.76rem;
  }

  .stm-result {
    display: grid;
    gap: 0.4rem;
    padding: 0.9rem 1rem;
    border-radius: 0.75rem;
    border: 1px solid #1e3a5f;
    background: #0a1020;
  }

  .stm-result.ok {
    border-color: #166534;
  }

  .stm-result.error {
    border-color: #7f1d1d;
  }

  .stm-result strong {
    color: #e2e8f0;
    font-size: 0.9rem;
  }

  .stm-result pre {
    margin: 0;
    white-space: pre-wrap;
    word-break: break-word;
    color: #a9bacd;
    font-size: 0.74rem;
    line-height: 1.45;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  }

  .label {
    color: #7a8da8;
    font-size: 0.74rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .info-card strong,
  .state-card strong {
    color: #e2e8f0;
    font-size: 0.95rem;
  }

  .info-card span,
  .state-card span {
    color: #a9bacd;
    font-size: 0.8rem;
    line-height: 1.4;
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

  .state-card.neutral {
    border-color: #1e3a5f;
  }

  .action {
    color: #dbeafe !important;
  }

  .muted {
    color: #94a3b8 !important;
  }

  .ok {
    color: #a8e2a1 !important;
  }

  .error {
    color: #f1aaaa !important;
  }

  .sensor-highlight {
    flex-shrink: 0;
  }

  .diagnostics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1rem;
    align-items: start;
  }

  @media (max-width: 900px) {
    .settings-shell {
      grid-template-columns: 1fr;
    }

    .diagnostics-grid,
    .info-grid,
    .diag-summary {
      grid-template-columns: 1fr;
    }
  }
</style>
