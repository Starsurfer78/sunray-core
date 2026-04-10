<script lang="ts">
  import { onMount } from "svelte";
  import PageLayout from "../components/PageLayout.svelte";
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
  import {
    getConfigDocument,
    updateConfigDocument,
    type ConfigUpdateResponse,
  } from "../api/rest";

  type Section = "general" | "diagnostics" | "robot" | "mqtt";

  let sidebarCollapsed = false;
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

  // Robot config state
  let robotConfig: Record<string, any> | null = null;
  let robotConfigDirty = false;
  let robotConfigError = "";
  let robotConfigSaving = false;

  // MQTT config state
  let mqttConfig: Record<string, any> | null = null;
  let mqttConfigDirty = false;
  let mqttConfigError = "";
  let mqttConfigSaving = false;

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
      stmUploadError =
        error instanceof Error
          ? error.message
          : "STM-Uploadstatus konnte nicht geladen werden";
    }
  }

  async function loadRobotConfig() {
    robotConfigError = "";
    try {
      const config = await getConfigDocument();
      robotConfig = config;
      robotConfigDirty = false;
    } catch (error) {
      robotConfigError =
        error instanceof Error
          ? error.message
          : "Konfiguration konnte nicht geladen werden";
    }
  }

  async function loadMqttConfig() {
    mqttConfigError = "";
    try {
      const config = await getConfigDocument();
      mqttConfig = config;
      mqttConfigDirty = false;
    } catch (error) {
      mqttConfigError =
        error instanceof Error
          ? error.message
          : "Konfiguration konnte nicht geladen werden";
    }
  }

  async function saveRobotConfig() {
    if (!robotConfig) return;
    robotConfigSaving = true;
    robotConfigError = "";
    try {
      await updateConfigDocument(robotConfig);
      robotConfigDirty = false;
    } catch (error) {
      robotConfigError =
        error instanceof Error ? error.message : "Speichern fehlgeschlagen";
    } finally {
      robotConfigSaving = false;
    }
  }

  async function saveMqttConfig() {
    if (!mqttConfig) return;
    mqttConfigSaving = true;
    mqttConfigError = "";
    try {
      await updateConfigDocument(mqttConfig);
      mqttConfigDirty = false;
    } catch (error) {
      mqttConfigError =
        error instanceof Error ? error.message : "Speichern fehlgeschlagen";
    } finally {
      mqttConfigSaving = false;
    }
  }

  onMount(() => {
    void loadStmUploadInfo();
    void loadRobotConfig();
    void loadMqttConfig();
  });

  async function runStmProbe() {
    stmProbeBusy = true;
    stmProbeError = "";
    stmProbeResult = null;
    try {
      stmProbeResult = await stmProbe();
      openStmDetail(stmProbeHeading, stmProbeResult.detail);
    } catch (error) {
      stmProbeError =
        error instanceof Error ? error.message : "STM-Probe fehlgeschlagen";
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
      stmUploadError =
        error instanceof Error
          ? error.message
          : "STM-Firmware konnte nicht hochgeladen werden";
    } finally {
      stmUploadBusy = false;
    }
  }

  async function handleStmFlash() {
    if (!stmUploadInfo?.exists || stmFlashBusy) return;
    if (
      !window.confirm("STM32 wirklich mit der hochgeladenen Firmware flashen?")
    )
      return;

    stmFlashBusy = true;
    stmFlashError = "";
    stmFlashResult = null;
    try {
      stmFlashResult = await flashUploadedStm();
      openStmDetail(
        stmFlashResult.ok
          ? "STM-Flash erfolgreich"
          : "STM-Flash fehlgeschlagen",
        stmFlashResult.detail,
      );
    } catch (error) {
      stmFlashError =
        error instanceof Error ? error.message : "STM-Flash fehlgeschlagen";
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
  $: stmFlashBlockedReason = !stmUploadInfo?.exists
    ? "Zuerst eine .bin hochladen."
    : !stmFlashAllowed
      ? "Flash nur aus Idle oder Charge."
      : "";
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="settings-page">
    <div class="hero">
      <div>
        <span class="eyebrow"
          >{activeSection === "general" ? "Allgemein" : "Diagnose"}</span
        >
        <h1>
          {activeSection === "general"
            ? "Systemstatus und Update"
            : "Hardware und Kalibrierung"}
        </h1>
        <p>
          {activeSection === "general"
            ? "OTA, Versionen und STM32-Firmware."
            : "Live-Sensorik und Servicefunktionen gebündelt."}
        </p>
      </div>
    </div>

    {#if activeSection === "general"}
      <section class="stats-grid">
        <article class="stat-card">
          <span class="label">Runtime</span>
          <strong>{$telemetry.runtime_health || "ok"}</strong>
          <span class="muted"
            >Op: {$telemetry.op || "—"} · Phase: {$telemetry.state_phase ||
              "—"}</span
          >
          <span class="muted">Resume: {$telemetry.resume_target || "—"}</span>
        </article>

        <article class="stat-card">
          <span class="label">Version</span>
          <strong>{$telemetry.pi_v || "—"}</strong>
          <span class="muted"
            >MCU: {$telemetry.mcu_v || "—"} · GPS: {$telemetry.gps_text ||
              "—"}</span
          >
          <span class="muted"
            >History: {$telemetry.history_backend_ready
              ? "bereit"
              : "nicht bereit"}</span
          >
        </article>

        <article class="stat-card">
          <span class="label">Letztes Ereignis</span>
          <strong>{humanizeReason($telemetry.event_reason)}</strong>
          <span class="muted">Fehler: {$telemetry.error_code || "—"}</span>
          <span class="muted">{recoveryNotice.title}</span>
        </article>
      </section>

      <div class="panel ota-wrap">
        <OtaUpdate />
      </div>

      <div class="panel">
        <div class="panel-header">
          <div>
            <span class="label">STM32 Firmware</span>
            <h2>SWD-Verbindung und Flash</h2>
          </div>
        </div>

        <div class="stm-steps">
          <div class="stm-step">
            <span class="step-num">1</span>
            <div class="step-body">
              <strong>SWD-Verbindung prüfen</strong>
              <p>
                Testet ob Alfred aktuell eine flashfähige Verbindung zum STM32
                aufbauen kann.
              </p>
              <div class="step-actions">
                <button
                  type="button"
                  class="btn"
                  disabled={stmProbeBusy}
                  on:click={runStmProbe}
                >
                  {stmProbeBusy ? "Prüfe …" : "SWD prüfen"}
                </button>
                {#if stmProbeResult || stmProbeError}
                  <button
                    type="button"
                    class="btn-ghost"
                    on:click={() =>
                      openStmDetail(
                        stmProbeHeading,
                        stmProbeResult?.detail ?? stmProbeError,
                      )}
                  >
                    Ausgabe ansehen
                  </button>
                {/if}
              </div>
              {#if stmProbeResult}
                <div
                  class="result-row"
                  class:ok={stmProbeResult.ok}
                  class:err={!stmProbeResult.ok}
                >
                  {stmProbeHeading}
                </div>
              {:else if stmProbeError}
                <div class="result-row err">Keine flashfähige Verbindung</div>
              {/if}
            </div>
          </div>

          <div class="stm-step">
            <span class="step-num">2</span>
            <div class="step-body">
              <strong>Firmware hochladen</strong>
              <p>.bin vom PC auf Alfred übertragen, ohne sofort zu flashen.</p>
              <div class="step-actions">
                <label class="file-label">
                  <input
                    type="file"
                    accept=".bin,application/octet-stream"
                    class="file-input"
                    on:change={(event) => {
                      const input = event.currentTarget as HTMLInputElement;
                      selectedStmFile = input.files?.[0] ?? null;
                    }}
                  />
                  <span
                    >{selectedStmFile
                      ? selectedStmFile.name
                      : "Datei wählen …"}</span
                  >
                </label>
                <button
                  type="button"
                  class="btn"
                  disabled={!selectedStmFile || stmUploadBusy}
                  on:click={handleStmUpload}
                >
                  {stmUploadBusy ? "Lädt hoch …" : "Hochladen"}
                </button>
              </div>
              {#if stmUploadError}
                <div class="result-row err">{stmUploadError}</div>
              {:else if stmUploadInfo?.exists}
                <div class="result-row ok">
                  {stmUploadInfo.original_name || "rm18-upload.bin"} ·
                  {Math.round((stmUploadInfo.size_bytes ?? 0) / 1024)} KB ·
                  {stmUploadInfo.uploaded_at_ms
                    ? new Date(stmUploadInfo.uploaded_at_ms).toLocaleString(
                        "de-DE",
                      )
                    : "—"}
                </div>
              {:else}
                <div class="result-row">Noch keine Firmware hochgeladen.</div>
              {/if}
            </div>
          </div>

          <div class="stm-step last">
            <span class="step-num">3</span>
            <div class="step-body">
              <strong>Hochgeladene Firmware flashen</strong>
              <p>
                Nur aus <code>Idle</code> oder <code>Charge</code> und nach erfolgreichem
                Upload.
              </p>
              <div class="step-actions">
                <button
                  type="button"
                  class="btn-danger"
                  disabled={!!stmFlashBlockedReason || stmFlashBusy}
                  on:click={handleStmFlash}
                >
                  {stmFlashBusy ? "Flashe …" : "Jetzt flashen"}
                </button>
                {#if stmFlashBlockedReason}
                  <span class="hint">{stmFlashBlockedReason}</span>
                {/if}
                {#if stmFlashResult}
                  <button
                    type="button"
                    class="btn-ghost"
                    on:click={() =>
                      openStmDetail(
                        stmFlashResult?.ok
                          ? "STM-Flash erfolgreich"
                          : "STM-Flash fehlgeschlagen",
                        stmFlashResult?.detail ?? "",
                      )}
                  >
                    Ausgabe ansehen
                  </button>
                {/if}
              </div>
              {#if stmFlashError}
                <div class="result-row err">{stmFlashError}</div>
              {/if}
            </div>
          </div>
        </div>
      </div>
    {:else if activeSection === "diagnostics"}
      <div class="diag-summary">
        <div class={`state-card ${recoveryNotice.tone}`}>
          <span class="label">Betriebszustand</span>
          <strong>{recoveryNotice.title}</strong>
          <span class="muted">{recoveryNotice.detail}</span>
          <span class="action-text">{recoveryNotice.action}</span>
        </div>
        <div class="state-card">
          <span class="label">Letzte Aktion</span>
          <strong>{$diagnostics.activeAction ?? "bereit"}</strong>
          {#if $diagnostics.error}
            <span class="text-err">{$diagnostics.error}</span>
          {:else if $diagnostics.lastResult}
            <span class="text-ok">
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
    {:else if activeSection === "robot"}
      <div class="hero">
        <div>
          <span class="eyebrow">Roboter-Einstellungen</span>
          <h1>Motor- und Dock-Konfiguration</h1>
          <p>
            Verhalten des Antriebssystems, automatischer Dock und
            Sensorkalibrierung.
          </p>
        </div>
      </div>

      {#if robotConfig}
        <section class="config-section">
          <div class="config-group">
            <h3>Antrieb</h3>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.invert_left_motor}
                  on:change={() => (robotConfigDirty = true)}
                />
                Linken Motor invertieren
              </label>
            </div>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.invert_right_motor}
                  on:change={() => (robotConfigDirty = true)}
                />
                Rechten Motor invertieren
              </label>
            </div>
            <div class="config-field">
              <label>
                Ticks pro Radumdrehung:
                <input
                  type="number"
                  bind:value={robotConfig.ticks_per_revolution}
                  on:change={() => (robotConfigDirty = true)}
                />
              </label>
            </div>
          </div>

          <div class="config-group">
            <h3>Dock</h3>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.dock_auto_start}
                  on:change={() => (robotConfigDirty = true)}
                />
                Nach Laden automatisch weitermähen
              </label>
            </div>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.dock_front_side}
                  on:change={() => (robotConfigDirty = true)}
                />
                Mit Vorderseite einkapseln
              </label>
            </div>
            <div class="config-field">
              <label>
                Max. Dock-Dauer (ms):
                <input
                  type="number"
                  bind:value={robotConfig.dock_max_duration_ms}
                  on:change={() => (robotConfigDirty = true)}
                />
              </label>
            </div>
          </div>

          <div class="config-group">
            <h3>Sensoren</h3>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.enable_lift_detection}
                  on:change={() => (robotConfigDirty = true)}
                />
                Hubsensor aktivwirt
              </label>
            </div>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.lift_obstacle_avoidance}
                  on:change={() => (robotConfigDirty = true)}
                />
                Hindernisausweichen beim Hubsensor
              </label>
            </div>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={robotConfig.enable_tilt_detection}
                  on:change={() => (robotConfigDirty = true)}
                />
                Kipp-Erkennung aktivieren
              </label>
            </div>
          </div>

          {#if robotConfigError}
            <div class="error-msg">{robotConfigError}</div>
          {/if}

          <div class="action-buttons">
            <button
              type="button"
              class="btn"
              disabled={!robotConfigDirty || robotConfigSaving}
              on:click={saveRobotConfig}
            >
              {robotConfigSaving ? "Speichert …" : "Speichern"}
            </button>
          </div>
        </section>
      {:else}
        <div class="error-msg">Konfiguration lädt …</div>
      {/if}
    {:else if activeSection === "mqtt"}
      <div class="hero">
        <div>
          <span class="eyebrow">MQTT-Integration</span>
          <h1>Telemetrie und Remote-Commands</h1>
          <p>Verbindung zu MQTT-Broker für Echtzeit-Daten und Steuerung.</p>
        </div>
      </div>

      {#if mqttConfig}
        <section class="config-section">
          <div class="config-group">
            <h3>Verbindung</h3>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={mqttConfig.mqtt_enabled}
                  on:change={() => (mqttConfigDirty = true)}
                />
                MQTT aktivieren
              </label>
            </div>
            <div class="config-field">
              <label>
                Host:
                <input
                  type="text"
                  bind:value={mqttConfig.mqtt_host}
                  on:change={() => (mqttConfigDirty = true)}
                />
              </label>
            </div>
            <div class="config-field">
              <label>
                Port:
                <input
                  type="number"
                  bind:value={mqttConfig.mqtt_port}
                  on:change={() => (mqttConfigDirty = true)}
                />
              </label>
            </div>
            <div class="config-field">
              <label>
                Keep-Alive (Sekunden):
                <input
                  type="number"
                  bind:value={mqttConfig.mqtt_keepalive_s}
                  on:change={() => (mqttConfigDirty = true)}
                />
              </label>
            </div>
          </div>

          <div class="config-group">
            <h3>Authentifizierung</h3>
            <div class="config-field">
              <label>
                Benutzername (optional):
                <input
                  type="text"
                  bind:value={mqttConfig.mqtt_user}
                  on:change={() => (mqttConfigDirty = true)}
                  placeholder="Leer für anonyme Verbindung"
                />
              </label>
            </div>
            <div class="config-field">
              <label>
                Passwort (optional):
                <input
                  type="password"
                  bind:value={mqttConfig.mqtt_pass}
                  on:change={() => (mqttConfigDirty = true)}
                />
              </label>
            </div>
          </div>

          <div class="config-group">
            <h3>Topics</h3>
            <div class="config-field">
              <label>
                Topic-Präfix:
                <input
                  type="text"
                  bind:value={mqttConfig.mqtt_topic_prefix}
                  on:change={() => (mqttConfigDirty = true)}
                />
              </label>
            </div>
          </div>

          <div class="config-group">
            <h3>Home Assistant Integration</h3>
            <div class="config-field">
              <label>
                <input
                  type="checkbox"
                  bind:checked={mqttConfig.mqtt_homeassistant_discovery}
                  on:change={() => (mqttConfigDirty = true)}
                />
                Home Assistant Discovery aktivieren
              </label>
            </div>
            <div class="config-field">
              <label>
                Discovery-Präfix:
                <input
                  type="text"
                  bind:value={mqttConfig.mqtt_homeassistant_prefix}
                  on:change={() => (mqttConfigDirty = true)}
                />
              </label>
            </div>
          </div>

          {#if mqttConfigError}
            <div class="error-msg">{mqttConfigError}</div>
          {/if}

          <div class="action-buttons">
            <button
              type="button"
              class="btn"
              disabled={!mqttConfigDirty || mqttConfigSaving}
              on:click={saveMqttConfig}
            >
              {mqttConfigSaving ? "Speichert …" : "Speichern"}
            </button>
          </div>
        </section>
      {:else}
        <div class="error-msg">Konfiguration lädt …</div>
      {/if}
    {/if}
  </div>

  <svelte:fragment slot="sidebar">
    <div class="sidebar-header">
      <span class="eyebrow">Einstellungen</span>
      <h2>System und Service</h2>
      <p>Selten genutzte Werkzeuge aus der Hauptnavigation.</p>
    </div>

    <div class="sr-sec">
      <div class="sr-lbl">Bereiche</div>
      <button
        type="button"
        class="nav-btn"
        class:active={activeSection === "general"}
        on:click={() => (activeSection = "general")}
      >
        <div class="nav-title">Allgemein</div>
        <div class="nav-sub">OTA, Runtime, Versionen</div>
      </button>
      <button
        type="button"
        class="nav-btn"
        class:active={activeSection === "diagnostics"}
        on:click={() => (activeSection = "diagnostics")}
      >
        <div class="nav-title">Diagnose</div>
        <div class="nav-sub">Sensoren, Kalibrierung, Motor-Tests</div>
      </button>
      <button
        type="button"
        class="nav-btn"
        class:active={activeSection === "robot"}
        on:click={() => (activeSection = "robot")}
      >
        <div class="nav-title">Roboter</div>
        <div class="nav-sub">Motor, Dock, Sensoren</div>
      </button>
      <button
        type="button"
        class="nav-btn"
        class:active={activeSection === "mqtt"}
        on:click={() => (activeSection = "mqtt")}
      >
        <div class="nav-title">MQTT</div>
        <div class="nav-sub">Telemetrie, Remote-Commands</div>
      </button>
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
      <div class="sr-lbl">Letzte Diag-Aktion</div>
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

{#if showStmDetail && stmDetailBody}
  <div
    class="probe-modal-backdrop"
    role="button"
    tabindex="0"
    aria-label="STM-Details schließen"
    on:click={() => (showStmDetail = false)}
    on:keydown={(event) => {
      if (
        event.key === "Escape" ||
        event.key === "Enter" ||
        event.key === " "
      ) {
        showStmDetail = false;
      }
    }}
  >
    <div
      class="probe-modal"
      role="dialog"
      tabindex="-1"
      aria-modal="true"
      aria-label="STM Details"
      on:click|stopPropagation
      on:keydown|stopPropagation
    >
      <div class="probe-modal-head">
        <div>
          <span class="eyebrow">STM32 Firmware</span>
          <h3>{stmDetailTitle}</h3>
        </div>
        <button
          type="button"
          class="btn-ghost"
          on:click={() => (showStmDetail = false)}
        >
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
  /* ── Page ───────────────────────────────────────────── */
  .settings-page {
    height: 100%;
    overflow-y: auto;
    padding: 1rem;
    display: grid;
    gap: 1rem;
    align-content: start;
    background: radial-gradient(
        circle at top right,
        rgba(30, 58, 95, 0.2),
        transparent 40%
      ),
      linear-gradient(180deg, rgba(7, 13, 24, 0.98), rgba(10, 15, 26, 0.98));
  }

  /* ── Hero ───────────────────────────────────────────── */
  .hero {
    padding: 1rem 1.1rem;
    border-radius: 1rem;
    background: rgba(15, 24, 41, 0.95);
    border: 1px solid #1e3a5f;
  }

  .eyebrow {
    display: inline-block;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
    margin-bottom: 0.35rem;
  }

  h1,
  h2,
  h3 {
    margin: 0;
  }

  h1 {
    font-size: 1.45rem;
    line-height: 1.1;
    color: #f8fafc;
  }
  h2 {
    font-size: 1rem;
    color: #e2e8f0;
  }

  .hero p {
    margin: 0.35rem 0 0;
    color: #94a3b8;
    font-size: 0.84rem;
    line-height: 1.45;
  }

  /* ── Stats grid ─────────────────────────────────────── */
  .stats-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 0.9rem;
  }

  .stat-card {
    display: grid;
    gap: 0.2rem;
    padding: 1rem;
    border-radius: 0.9rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .stat-card strong {
    font-size: 1.1rem;
    color: #f8fafc;
    margin-top: 0.1rem;
  }

  /* ── Panel ──────────────────────────────────────────── */
  .panel {
    background: #0f1829;
    border: 1px solid #1e3a5f;
    border-radius: 0.9rem;
    padding: 1rem;
  }

  .ota-wrap {
    padding: 0;
    overflow: hidden;
  }

  .panel-header {
    display: flex;
    justify-content: space-between;
    align-items: end;
    gap: 0.75rem;
    margin-bottom: 0.9rem;
  }

  /* ── STM steps ──────────────────────────────────────── */
  .stm-steps {
    display: grid;
    border-radius: 0.75rem;
    border: 1px solid #1e3a5f;
    overflow: hidden;
  }

  .stm-step {
    display: grid;
    grid-template-columns: 2rem 1fr;
    gap: 0.9rem;
    padding: 1rem;
    border-bottom: 1px solid #1e3a5f;
    background: rgba(10, 16, 32, 0.6);
    align-items: start;
  }

  .stm-step.last {
    border-bottom: none;
  }

  .step-num {
    width: 2rem;
    height: 2rem;
    border-radius: 50%;
    background: #0a1020;
    border: 1px solid #1e3a5f;
    color: #60a5fa;
    font-size: 0.8rem;
    font-weight: 700;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    margin-top: 0.1rem;
  }

  .step-body {
    display: grid;
    gap: 0.55rem;
  }

  .step-body strong {
    color: #e2e8f0;
    font-size: 0.92rem;
  }

  .step-body p {
    margin: 0;
    color: #64748b;
    font-size: 0.82rem;
    line-height: 1.4;
  }

  .step-body code {
    color: #93c5fd;
    font-size: 0.8rem;
  }

  .step-actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.5rem;
  }

  .result-row {
    padding: 0.55rem 0.75rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #070d18;
    color: #94a3b8;
    font-size: 0.82rem;
  }

  .result-row.ok {
    border-color: #166534;
    color: #86efac;
  }
  .result-row.err {
    border-color: #7f1d1d;
    color: #fca5a5;
  }

  /* ── Buttons ────────────────────────────────────────── */
  .btn {
    padding: 0.55rem 0.9rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #10203a;
    color: #dbeafe;
    cursor: pointer;
    font-weight: 600;
    font-size: 0.84rem;
  }

  .btn:disabled {
    opacity: 0.55;
    cursor: not-allowed;
  }
  .btn:not(:disabled):hover {
    border-color: #3b82f6;
  }

  .btn-ghost {
    padding: 0.55rem 0.9rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: transparent;
    color: #7a8da8;
    cursor: pointer;
    font-size: 0.84rem;
  }

  .btn-ghost:hover {
    border-color: #3b82f6;
    color: #93c5fd;
  }

  .btn-danger {
    padding: 0.55rem 0.9rem;
    border-radius: 0.55rem;
    border: 1px solid #7f1d1d;
    background: #1c0a0a;
    color: #fca5a5;
    cursor: pointer;
    font-weight: 600;
    font-size: 0.84rem;
  }

  .btn-danger:not(:disabled):hover {
    border-color: #ef4444;
    color: #fecaca;
  }
  .btn-danger:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .hint {
    color: #64748b;
    font-size: 0.78rem;
  }

  /* ── File input ─────────────────────────────────────── */
  .file-label {
    display: inline-flex;
    align-items: center;
    padding: 0.55rem 0.9rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #0a1020;
    color: #7a8da8;
    cursor: pointer;
    font-size: 0.82rem;
    max-width: 240px;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }

  .file-label:hover {
    border-color: #3b82f6;
    color: #93c5fd;
  }

  .file-input {
    position: absolute;
    width: 1px;
    height: 1px;
    opacity: 0;
    overflow: hidden;
    clip: rect(0 0 0 0);
  }

  /* ── Diag summary ───────────────────────────────────── */
  .diag-summary {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 0.9rem;
  }

  .state-card {
    display: grid;
    gap: 0.2rem;
    padding: 1rem;
    border-radius: 0.9rem;
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

  .state-card strong {
    color: #e2e8f0;
    font-size: 0.95rem;
  }

  .action-text {
    color: #dbeafe;
    font-size: 0.82rem;
  }
  .text-ok {
    color: #86efac;
    font-size: 0.82rem;
  }
  .text-err {
    color: #fca5a5;
    font-size: 0.82rem;
  }

  .diagnostics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1rem;
    align-items: start;
  }

  /* ── Shared text ────────────────────────────────────── */
  .label {
    color: #7a8da8;
    font-size: 0.74rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .muted {
    color: #94a3b8;
    font-size: 0.82rem;
    line-height: 1.4;
  }

  /* ── Sidebar ────────────────────────────────────────── */
  .sidebar-header {
    padding: 1rem;
    border-bottom: 1px solid #0f1829;
    display: grid;
    gap: 0.3rem;
  }

  .sidebar-header h2 {
    font-size: 1.1rem;
    color: #e2e8f0;
  }

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
    gap: 0.4rem;
    flex-shrink: 0;
  }

  .sr-lbl {
    font-size: 10px;
    font-weight: 500;
    color: #475569;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 3px;
  }

  .nav-btn {
    display: grid;
    gap: 0.15rem;
    padding: 0.6rem 0.75rem;
    border-radius: 0.5rem;
    border: 1px solid transparent;
    background: transparent;
    color: #94a3b8;
    cursor: pointer;
    text-align: left;
    transition:
      background 120ms,
      border-color 120ms;
  }

  .nav-btn:hover {
    background: #0f1829;
    border-color: #1e3a5f;
  }

  .nav-btn.active {
    background: #0f1829;
    border-color: #3b82f6;
    color: #dbeafe;
  }

  .nav-title {
    font-size: 0.84rem;
    font-weight: 600;
  }

  .nav-sub {
    font-size: 0.72rem;
    color: #475569;
    line-height: 1.3;
  }

  .nav-btn.active .nav-sub {
    color: #7a8da8;
  }

  .sr-stbig {
    font-size: 14px;
    font-weight: 500;
    color: #60a5fa;
  }
  .sr-detail {
    font-size: 11px;
    color: #64748b;
    line-height: 1.4;
  }
  .sr-action {
    font-size: 11px;
    color: #dbeafe;
  }
  .sr-ok {
    font-size: 11px;
    color: #86efac;
  }
  .sr-err {
    font-size: 11px;
    color: #fca5a5;
  }
  .sr-muted {
    font-size: 11px;
    color: #64748b;
  }

  /* ── Config sections ────────────────────────────────── */
  .config-section {
    padding: 1rem;
    border-radius: 1rem;
    background: rgba(15, 24, 41, 0.95);
    border: 1px solid #1e3a5f;
    display: grid;
    gap: 1.2rem;
  }

  .config-group {
    display: grid;
    gap: 0.75rem;
  }

  .config-group h3 {
    font-size: 0.95rem;
    color: #e2e8f0;
    margin: 0;
    padding-bottom: 0.35rem;
    border-bottom: 1px solid #1e3a5f;
  }

  .config-field {
    display: grid;
    gap: 0.3rem;
  }

  .config-field label {
    display: flex;
    align-items: center;
    gap: 0.65rem;
    color: #cbd5e1;
    font-size: 0.85rem;
  }

  .config-field label input[type="checkbox"] {
    cursor: pointer;
    width: 1.1rem;
    height: 1.1rem;
    accent-color: #60a5fa;
  }

  .config-field label input[type="text"],
  .config-field label input[type="password"],
  .config-field label input[type="number"] {
    flex: 1;
    min-width: 0;
    padding: 0.45rem 0.65rem;
    border-radius: 0.45rem;
    background: #0b1220;
    border: 1px solid #1e3a5f;
    color: #f1f5f9;
    font-size: 0.85rem;
  }

  .config-field label input[type="text"]:focus,
  .config-field label input[type="password"]:focus,
  .config-field label input[type="number"]:focus {
    outline: none;
    border-color: #3b82f6;
    box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.1);
  }

  .config-field label input::placeholder {
    color: #64748b;
  }

  .error-msg {
    padding: 0.75rem 1rem;
    border-radius: 0.45rem;
    background: rgba(239, 68, 68, 0.1);
    border: 1px solid rgba(239, 68, 68, 0.3);
    color: #fca5a5;
    font-size: 0.85rem;
  }

  .action-buttons {
    display: flex;
    gap: 0.65rem;
    padding-top: 0.5rem;
    border-top: 1px solid #1e3a5f;
  }

  /* ── Modal ──────────────────────────────────────────── */
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

  /* ── Responsive ─────────────────────────────────────── */
  @media (max-width: 1000px) {
    .stats-grid {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
  }

  @media (max-width: 680px) {
    .stats-grid,
    .diag-summary,
    .diagnostics-grid {
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
