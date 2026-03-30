<script lang="ts">
  import { runMotorDiag, updateMotorCalibrationConfig } from '../../api/rest'
  import { diagnostics } from '../../stores/diagnostics'

  type Wheel = 'left' | 'right'

  let selectedWheel: Wheel = 'left'
  let pwm = 0.15
  let measuredTicks: number | null = null
  let savedTicks: number | null = null

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
      Das gewaehlte Rad dreht sich genau 1x. Die gemessenen Encoder-Ticks werden
      angezeigt und koennen als <code>ticks_per_revolution</code> gespeichert werden.
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

  <div class="result-grid">
    <article class="card">
      <span>Messwert</span>
      <strong>{measuredTicks ?? '---'}</strong>
    </article>
    <article class="card">
      <span>Gespeichert</span>
      <strong>{savedTicks ?? '---'}</strong>
    </article>
    <article class="card">
      <span>Hinweis</span>
      <strong>Soll: genau 1 Radumdrehung</strong>
    </article>
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header,
  .controls,
  .actions,
  .result-grid {
    display: grid;
    gap: 0.8rem;
  }

  h2,
  p {
    margin: 0;
  }

  p {
    color: #9db3ab;
  }

  code {
    color: #d8e9a8;
  }

  .controls {
    grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  }

  label {
    display: grid;
    gap: 0.3rem;
    color: #c8d9d2;
  }

  select,
  input {
    width: 100%;
    padding: 0.7rem 0.8rem;
    border: 1px solid rgba(152, 187, 170, 0.2);
    border-radius: 0.8rem;
    background: rgba(24, 38, 34, 0.85);
    color: #dce8e8;
  }

  .actions {
    grid-template-columns: repeat(auto-fit, minmax(180px, max-content));
  }

  button {
    padding: 0.8rem 1rem;
    border: 0;
    border-radius: 0.9rem;
    cursor: pointer;
    background: #d9c17e;
    color: #07110f;
    font-weight: 700;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .result-grid {
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  }

  .card {
    display: grid;
    gap: 0.35rem;
    padding: 0.9rem;
    border-radius: 0.9rem;
    background: rgba(24, 38, 34, 0.8);
    border: 1px solid rgba(152, 187, 170, 0.12);
  }
</style>
