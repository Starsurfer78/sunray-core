<script lang="ts">
  import { onMount } from 'svelte'
  import { getConfigDocument, runMotorDiag, updateMotorCalibrationConfig } from '../../api/rest'
  import { diagnostics } from '../../stores/diagnostics'

  type Wheel = 'left' | 'right'

  let selectedWheel: Wheel = 'left'
  let pwm = 0.15
  let measuredTicks: number | null = null
  let savedTicks: number | null = null
  let manualTicks: number | null = null

  onMount(async () => {
    try {
      const config = await getConfigDocument()
      savedTicks = typeof config.ticks_per_revolution === 'number' ? config.ticks_per_revolution : null
      manualTicks = savedTicks
    } catch {
      savedTicks = null
      manualTicks = null
    }
  })

  async function measureOneRevolution() {
    diagnostics.start(`ticks:${selectedWheel}`)
    try {
      const result = await runMotorDiag({
        motor: selectedWheel,
        pwm,
        duration_ms: 8000,
        revolutions: 1,
      })

      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Tick-Kalibrierung fehlgeschlagen')
        return
      }

      measuredTicks = result.ticks ?? null
      diagnostics.success(result)
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Tick-Kalibrierung fehlgeschlagen')
    }
  }

  async function saveMeasuredTicks() {
    if (measuredTicks === null) return

    diagnostics.start('config:ticks_per_revolution')
    try {
      const rounded = Math.max(1, Math.round(measuredTicks))
      const result = await updateMotorCalibrationConfig({
        ticks_per_revolution: rounded,
      })

      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Ticks konnten nicht gespeichert werden')
        return
      }

      savedTicks = rounded
      manualTicks = rounded
      diagnostics.success({ ok: true, message: `ticks_per_revolution = ${rounded} gespeichert` })
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Ticks konnten nicht gespeichert werden')
    }
  }

  async function saveManualTicks() {
    if (manualTicks === null) return

    diagnostics.start('config:ticks_per_revolution_manual')
    try {
      const rounded = Math.max(1, Math.round(manualTicks))
      const result = await updateMotorCalibrationConfig({
        ticks_per_revolution: rounded,
      })

      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Ticks konnten nicht gespeichert werden')
        return
      }

      savedTicks = rounded
      manualTicks = rounded
      diagnostics.success({ ok: true, message: `ticks_per_revolution = ${rounded} gespeichert` })
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Ticks konnten nicht gespeichert werden')
    }
  }
</script>

<section class="panel">
  <header>
    <h2>Tick-Kalibrierung</h2>
    <p>
      Das gewählte Rad dreht sich genau 1x. Die gemessenen Encoder-Ticks werden
      angezeigt und können als <code>ticks_per_revolution</code> gespeichert werden.
    </p>
  </header>

  <div class="controls">
    <label>
      Rad
      <select bind:value={selectedWheel}>
        <option value="left">Links</option>
        <option value="right">Rechts</option>
      </select>
    </label>

    <label>
      PWM
      <input type="number" min="0.05" max="1" step="0.01" bind:value={pwm} />
    </label>
  </div>

  <div class="actions">
    <button type="button" disabled={$diagnostics.busy} on:click={measureOneRevolution}>
      {$diagnostics.activeAction?.startsWith('ticks:') ? 'Messe ...' : '1 Umdrehung messen'}
    </button>

    <button type="button" disabled={$diagnostics.busy || measuredTicks === null} on:click={saveMeasuredTicks}>
      Messwert speichern
    </button>
  </div>

  <div class="manual-save">
    <label>
      Ticks pro Umdrehung
      <input type="number" min="1" step="1" bind:value={manualTicks} />
    </label>

    <button
      type="button"
      disabled={$diagnostics.busy || manualTicks === null}
      on:click={saveManualTicks}
    >
      Direkt speichern
    </button>
  </div>

  <div class="result-grid">
    <article class="card">
      <span>Messwert</span>
      <strong>{measuredTicks ?? '---'}</strong>
    </article>
    <article class="card">
      <span>Gespeichert</span>
      <strong>{savedTicks ?? '---'}</strong>
    </article>
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 0.6rem;
    padding: 0.75rem 0.85rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  header,
  .controls,
  .actions,
  .result-grid {
    display: grid;
    gap: 0.45rem;
  }

  h2, p { margin: 0; }

  h2 { font-size: 0.92rem; color: #e2e8f0; }

  p { color: #64748b; font-size: 0.78rem; }

  code { color: #93c5fd; font-size: 0.78rem; }

  .controls {
    grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  }

  label {
    display: grid;
    gap: 0.2rem;
    color: #94a3b8;
    font-size: 0.78rem;
  }

  select,
  input {
    width: 100%;
    padding: 0.35rem 0.55rem;
    border: 1px solid #1a2a40;
    border-radius: 0.4rem;
    background: #08101d;
    color: #dce8e8;
    font-size: 0.82rem;
  }

  .actions {
    grid-template-columns: repeat(auto-fit, minmax(160px, max-content));
    gap: 0.4rem;
  }

  .manual-save {
    display: grid;
    gap: 0.4rem;
    grid-template-columns: minmax(140px, 200px) max-content;
    align-items: end;
  }

  button {
    padding: 0.4rem 0.7rem;
    border: 1px solid #2563eb;
    border-radius: 0.45rem;
    cursor: pointer;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 600;
    font-size: 0.82rem;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .result-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 0.45rem;
  }

  @media (max-width: 720px) {
    .manual-save {
      grid-template-columns: 1fr;
    }
  }

  .card {
    display: grid;
    gap: 0.15rem;
    padding: 0.5rem 0.65rem;
    border-radius: 0.55rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
  }

  .card span { font-size: 0.74rem; color: #64748b; }

  .card strong { font-size: 0.88rem; color: #93c5fd; font-family: ui-monospace, monospace; }
</style>
