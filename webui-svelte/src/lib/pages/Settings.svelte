<script lang="ts">
  import { onMount } from "svelte";
  import OtaUpdate from "../components/OtaUpdate.svelte";
  import {
    flashUploadedStm,
    getStmUploadedFirmware,
    stmProbe,
    uploadStmFirmware,
    type StmFlashResponse,
    type StmProbeResponse,
    type StmUploadedFirmwareInfo,
  } from "../api/rest";
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
  let stmUploadBusy = false;
  let stmUploadInfo: StmUploadedFirmwareInfo | null = null;
  let stmUploadError = "";
  let stmFlashBusy = false;
  let stmFlashResult: StmFlashResponse | null = null;
  let stmFlashError = "";
  let selectedStmFile: File | null = null;
  let showStmDetail = false;
  let stmDetailTitle = "";
  let stmDetailBody = "";

  $: recoveryNotice = getRecoveryNotice($telemetry);

  function openStmDetail(title: string, body: string) {
    stmDetailTitle = title;
    stmDetailBody = body;
    showStmDetail = true;
  }

  async function loadStmUploadInfo() {
    stmUploadError = "";
    try {
      stmUploadInfo = await getStmUploadedFirmware();
    } catch (error) {
      stmUploadError = error instanceof Error ? error.message : "STM-Uploadstatus konnte nicht geladen werden";
    }
  }

  onMount(() => {
    void loadStmUploadInfo();
  });

  async function runStmProbe() {
    stmProbeBusy = true;
    stmProbeError = "";
    stmProbeResult = null;
    try {
      stmProbeResult = await stmProbe();
      openStmDetail(stmProbeHeading, stmProbeResult.detail);
    } catch (error) {
      stmProbeError = error instanceof Error ? error.message : "STM-Probe fehlgeschlagen";
      openStmDetail("Keine flashfähige Verbindung", stmProbeError);
    } finally {
      stmProbeBusy = false;
    }
  }

  async function handleStmUpload() {
    if (!selectedStmFile) return;

    stmUploadBusy = true;
    stmUploadError = "";
    try {
      stmUploadInfo = await uploadStmFirmware(selectedStmFile);
      await loadStmUploadInfo();
      selectedStmFile = null;
    } catch (error) {
      stmUploadError = error instanceof Error ? error.message : "STM-Firmware konnte nicht hochgeladen werden";
    } finally {
      stmUploadBusy = false;
    }
  }

  async function handleStmFlash() {
    if (!stmUploadInfo?.exists || stmFlashBusy) return;
    if (!window.confirm("STM32 wirklich mit der hochgeladenen Firmware flashen?")) return;

    stmFlashBusy = true;
    stmFlashError = "";
    stmFlashResult = null;
    try {
      stmFlashResult = await flashUploadedStm();
      openStmDetail(
        stmFlashResult.ok ? "STM-Flash erfolgreich" : "STM-Flash fehlgeschlagen",
        stmFlashResult.detail,
      );
    } catch (error) {
      stmFlashError = error instanceof Error ? error.message : "STM-Flash fehlgeschlagen";
      openStmDetail("STM-Flash fehlgeschlagen", stmFlashError);
    } finally {
      stmFlashBusy = false;
    }
  }

  $: stmProbeHeading =
    stmProbeResult?.status === "probe_tool_missing"
      ? "OpenOCD fehlt auf Alfred"
      : stmProbeResult?.ok
        ? "Flashfähige Verbindung erkannt"
        : "Keine flashfähige Verbindung";
  $: stmFlashAllowed = $telemetry.op === "Idle" || $telemetry.op === "Charge";
  $: stmFlashBlockedReason =
    !stmUploadInfo?.exists
      ? "Zuerst eine .bin hochladen."
      : !stmFlashAllowed
        ? "Flash nur aus Idle oder Charge."
        : "";
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
          <div class="stm-result" class:ok={stmProbeResult.ok} class:error={!stmProbeResult.ok}>
            <strong>{stmProbeHeading}</strong>
            <div class="stm-result-actions">
              <span>Ausgabe bereit</span>
              <button
                type="button"
                class="detail-btn"
                on:click={() => openStmDetail(stmProbeHeading, stmProbeResult?.detail || "")}
              >
                Details öffnen
              </button>
            </div>
          </div>
        {/if}

        {#if stmProbeError}
          <div class="stm-result error">
            <strong>Keine flashfähige Verbindung</strong>
            <div class="stm-result-actions">
              <span>Fehlerausgabe bereit</span>
              <button
                type="button"
                class="detail-btn"
                on:click={() => openStmDetail("Keine flashfähige Verbindung", stmProbeError)}
              >
                Details öffnen
              </button>
            </div>
          </div>
        {/if}

        <div class="stm-upload-box">
          <div class="stm-upload-copy">
            <strong>Firmware hochladen</strong>
            <span>.bin vom PC auf Alfred ablegen, ohne sofort zu flashen.</span>
          </div>
          <div class="stm-actions">
            <input
              type="file"
              accept=".bin,application/octet-stream"
              on:change={(event) => {
                const input = event.currentTarget as HTMLInputElement;
                selectedStmFile = input.files?.[0] ?? null;
              }}
            />
            <button
              type="button"
              class="probe-btn"
              disabled={!selectedStmFile || stmUploadBusy}
              on:click={handleStmUpload}
            >
              {stmUploadBusy ? "Lade hoch ..." : "Upload .bin"}
            </button>
          </div>
          {#if selectedStmFile}
            <span class="stm-note">
              Gewählt: {selectedStmFile.name} ({Math.round(selectedStmFile.size / 1024)} KB)
            </span>
          {/if}
          {#if stmUploadError}
            <div class="stm-result error">
              <strong>Upload fehlgeschlagen</strong>
              <span>{stmUploadError}</span>
            </div>
          {/if}
        </div>

        <div class="stm-upload-box">
          <div class="stm-upload-copy">
            <strong>Hochgeladene Firmware</strong>
            <span>Fixer Speicherort fuer den naechsten kontrollierten Flash.</span>
          </div>
          {#if stmUploadInfo?.exists}
            <div class="stm-result ok">
              <strong>{stmUploadInfo.original_name || "rm18-upload.bin"}</strong>
              <span>
                {Math.round((stmUploadInfo.size_bytes ?? 0) / 1024)} KB ·
                {stmUploadInfo.uploaded_at_ms ? new Date(stmUploadInfo.uploaded_at_ms).toLocaleString("de-DE") : "—"}
              </span>
              <span>{stmUploadInfo.stored_path || "—"}</span>
            </div>
          {:else}
            <div class="stm-result">
              <strong>Keine Firmware hochgeladen</strong>
              <span>Der Upload legt die Datei unter `/var/lib/sunray-core/stm-upload/` ab.</span>
            </div>
          {/if}
        </div>

        <div class="stm-upload-box">
          <div class="stm-upload-copy">
            <strong>Hochgeladene Firmware flashen</strong>
            <span>Nur nach erfolgreichem Upload und nur aus `Idle` oder `Charge`.</span>
          </div>
          <div class="stm-actions">
            <button
              type="button"
              class="probe-btn"
              disabled={!!stmFlashBlockedReason || stmFlashBusy}
              on:click={handleStmFlash}
            >
              {stmFlashBusy ? "Flashe STM ..." : "Hochgeladene Firmware flashen"}
            </button>
            <span class="stm-note">
              {stmFlashBlockedReason || "Flash-Ausgabe erscheint im gleichen Terminalfenster wie beim Probe."}
            </span>
          </div>
          {#if stmFlashError}
            <div class="stm-result error">
              <strong>STM-Flash fehlgeschlagen</strong>
              <span>{stmFlashError}</span>
            </div>
          {/if}
        </div>
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

{#if showStmDetail && stmDetailBody}
  <div
    class="probe-modal-backdrop"
    role="button"
    tabindex="0"
    aria-label="STM-Probe-Details schließen"
    on:click={() => (showStmDetail = false)}
    on:keydown={(event) => {
      if (event.key === "Escape" || event.key === "Enter" || event.key === " ") {
        showStmDetail = false;
      }
    }}
  >
    <div
      class="probe-modal"
      role="dialog"
      tabindex="-1"
      aria-modal="true"
      aria-label="STM Probe Details"
      on:click|stopPropagation
      on:keydown|stopPropagation
    >
      <div class="probe-modal-head">
        <div>
          <span class="eyebrow">STM32 Firmware</span>
          <h3>{stmDetailTitle}</h3>
        </div>
        <button type="button" class="detail-btn close-btn" on:click={() => (showStmDetail = false)}>
          Schließen
        </button>
      </div>
      <div class="probe-terminal">
        <div class="probe-terminal-bar">
          <span class="dot red"></span>
          <span class="dot yellow"></span>
          <span class="dot green"></span>
          <span class="probe-terminal-title">stm-action.log</span>
        </div>
        <pre class="probe-log">{stmDetailBody}</pre>
      </div>
    </div>
  </div>
{/if}

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

  .stm-upload-box {
    display: grid;
    gap: 0.55rem;
    padding: 0.9rem 1rem;
    border-radius: 0.8rem;
    border: 1px solid #1e3a5f;
    background: #0a1020;
  }

  .stm-upload-copy {
    display: grid;
    gap: 0.2rem;
  }

  .stm-upload-copy strong {
    color: #e2e8f0;
    font-size: 0.92rem;
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

  .stm-result-actions {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.75rem;
    color: #cbd5e1;
    font-size: 0.8rem;
  }

  .detail-btn {
    padding: 0.45rem 0.75rem;
    border-radius: 0.55rem;
    border: 1px solid #335c8b;
    background: #10213b;
    color: #dbeafe;
    cursor: pointer;
    font: inherit;
  }

  .detail-btn:hover {
    border-color: #60a5fa;
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

  .probe-modal-backdrop {
    position: fixed;
    inset: 0;
    z-index: 40;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 1rem;
    background: rgba(2, 6, 23, 0.72);
  }

  .probe-modal {
    width: min(860px, 100%);
    max-height: min(82vh, 760px);
    display: grid;
    grid-template-rows: auto minmax(0, 1fr);
    gap: 0.8rem;
    padding: 1rem;
    border-radius: 0.95rem;
    border: 1px solid #1e3a5f;
    background: #08111f;
    box-shadow: 0 24px 60px rgba(0, 0, 0, 0.45);
  }

  .probe-modal-head {
    display: flex;
    align-items: start;
    justify-content: space-between;
    gap: 1rem;
  }

  .close-btn {
    white-space: nowrap;
  }

  .probe-log {
    margin: 0;
    min-height: 0;
    overflow: auto;
    padding: 0.95rem 1rem 1rem;
    background: #020617;
    white-space: pre-wrap;
    word-break: break-word;
    color: #c7f9cc;
    font-size: 0.78rem;
    line-height: 1.5;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  }

  .probe-terminal {
    min-height: 0;
    display: grid;
    grid-template-rows: auto minmax(0, 1fr);
    border-radius: 0.8rem;
    border: 1px solid #14243f;
    overflow: hidden;
    background: #020617;
  }

  .probe-terminal-bar {
    display: flex;
    align-items: center;
    gap: 0.45rem;
    padding: 0.65rem 0.85rem;
    background: #0b1220;
    border-bottom: 1px solid #14243f;
  }

  .dot {
    width: 0.68rem;
    height: 0.68rem;
    border-radius: 999px;
    display: inline-block;
  }

  .dot.red {
    background: #ef4444;
  }

  .dot.yellow {
    background: #f59e0b;
  }

  .dot.green {
    background: #22c55e;
  }

  .probe-terminal-title {
    margin-left: 0.35rem;
    color: #94a3b8;
    font-size: 0.76rem;
    letter-spacing: 0.04em;
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

    .probe-modal {
      max-height: 88vh;
      padding: 0.9rem;
    }

    .probe-modal-head {
      flex-direction: column;
      align-items: stretch;
    }
  }
</style>
