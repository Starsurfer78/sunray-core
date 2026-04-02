<script lang="ts">
  import { telemetry } from "../stores/telemetry";
  import { otaCheck, otaUpdate, type OtaCheckResponse } from "../api/rest";

  let checkResult: OtaCheckResponse | null = null;
  let busy = false;
  let errorMsg = "";
  // After update_started the service restarts — we poll until the UI reconnects.
  let waitingForRestart = false;
  let restartCountdown = 120;
  let countdownTimer: ReturnType<typeof setInterval> | null = null;

  async function handleCheck() {
    busy = true;
    errorMsg = "";
    checkResult = null;
    try {
      checkResult = await otaCheck();
    } catch (err) {
      errorMsg = err instanceof Error ? err.message : "Fehler beim Prüfen";
    } finally {
      busy = false;
    }
  }

  async function handleUpdate() {
    if (!confirm("Roboter wird nach dem Update neu gestartet. Fortfahren?")) return;
    busy = true;
    errorMsg = "";
    try {
      await otaUpdate();
      waitingForRestart = true;
      restartCountdown = 120;
      countdownTimer = setInterval(() => {
        restartCountdown -= 1;
        if (restartCountdown <= 0) {
          clearInterval(countdownTimer!);
          countdownTimer = null;
          window.location.reload();
        }
      }, 1000);
    } catch (err) {
      errorMsg = err instanceof Error ? err.message : "Fehler beim Starten des Updates";
      busy = false;
    }
  }
</script>

<div class="ota-panel">
  <div class="ota-title">Software-Update</div>

  <div class="ota-version-row">
    <span class="ota-label">Aktuelle Version</span>
    <strong class="ota-value">{$telemetry.pi_v || "—"}</strong>
  </div>

  {#if waitingForRestart}
    <div class="ota-status restarting">
      Update läuft — Roboter startet neu…
      <span class="ota-countdown">Seite lädt in {restartCountdown}s</span>
    </div>
  {:else}
    <div class="ota-actions">
      <button class="ota-btn" on:click={handleCheck} disabled={busy}>
        Auf Update prüfen
      </button>

      {#if checkResult}
        {#if checkResult.status === "up_to_date"}
          <span class="ota-badge ok">Aktuell</span>
        {:else if checkResult.status === "update_available"}
          <span class="ota-badge new">Neu: {checkResult.hash}</span>
          <button class="ota-btn primary" on:click={handleUpdate} disabled={busy}>
            Jetzt installieren
          </button>
        {:else if checkResult.status === "error"}
          <span class="ota-badge err">{checkResult.detail ?? "Fehler"}</span>
        {/if}
      {/if}
    </div>

    {#if errorMsg}
      <div class="ota-error">{errorMsg}</div>
    {/if}
  {/if}
</div>

<style>
  .ota-panel {
    display: grid;
    gap: 0.5rem;
    padding: 0.7rem 0.8rem;
    border-top: 1px solid #0f1829;
  }

  .ota-title {
    color: #60a5fa;
    font-size: 0.62rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .ota-version-row {
    display: flex;
    align-items: baseline;
    gap: 0.5rem;
  }

  .ota-label {
    color: #64748b;
    font-size: 0.68rem;
  }

  .ota-value {
    color: #94a3b8;
    font-size: 0.72rem;
    font-family: monospace;
  }

  .ota-actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.45rem;
  }

  .ota-btn {
    padding: 0.42rem 0.75rem;
    border-radius: 0.45rem;
    border: 1px solid #1e3a5f;
    background: #0a1020;
    color: #93c5fd;
    font-size: 0.7rem;
    cursor: pointer;
  }

  .ota-btn:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .ota-btn.primary {
    border-color: #2563eb;
    background: #0c1a3a;
    font-weight: 700;
  }

  .ota-badge {
    font-size: 0.68rem;
    padding: 0.2rem 0.5rem;
    border-radius: 0.35rem;
  }

  .ota-badge.ok   { background: #052e16; color: #4ade80; }
  .ota-badge.new  { background: #1e3a5f; color: #7dd3fc; }
  .ota-badge.err  { background: #450a0a; color: #fca5a5; }

  .ota-status.restarting {
    display: grid;
    gap: 0.2rem;
    color: #fbbf24;
    font-size: 0.72rem;
  }

  .ota-countdown {
    color: #64748b;
    font-size: 0.64rem;
  }

  .ota-error {
    color: #fca5a5;
    font-size: 0.68rem;
  }
</style>
