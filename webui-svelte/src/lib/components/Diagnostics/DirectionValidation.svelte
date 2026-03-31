<script lang="ts">
  import { runMotorDiag, updateMotorCalibrationConfig } from '../../api/rest'
  import { diagnostics } from '../../stores/diagnostics'

  type Wheel = 'left' | 'right'

  let selectedWheel: Wheel = 'left'
  let pwm = 0.12
  let durationMs = 900
  let lastDecision = ''
  let hasPulseResult = false

  async function pulseForward() {
    hasPulseResult = false
    lastDecision = ''
    diagnostics.start(`direction:${selectedWheel}`)
    try {
      const result = await runMotorDiag({
        motor: selectedWheel,
        pwm,
        duration_ms: durationMs,
      })

      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Richtungspruefung fehlgeschlagen')
        return
      }

      hasPulseResult = true
      diagnostics.success(result)
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Richtungspruefung fehlgeschlagen')
    }
  }

  async function saveDirection(isCorrectForward: boolean) {
    if (!hasPulseResult) {
      diagnostics.fail('Erst einen Vorwaertsimpuls ausfuehren und die Richtung beobachten')
      return
    }
    diagnostics.start(`config:invert:${selectedWheel}`)
    try {
      const payload =
        selectedWheel === 'left'
          ? { invert_left_motor: !isCorrectForward }
          : { invert_right_motor: !isCorrectForward }

      const result = await updateMotorCalibrationConfig(payload)
      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Richtungswert konnte nicht gespeichert werden')
        return
      }

      lastDecision = isCorrectForward
        ? `${selectedWheel} vorwaerts ist korrekt`
        : `${selectedWheel} wird invertiert`
      hasPulseResult = false
      diagnostics.success({ ok: true, message: lastDecision })
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Richtungswert konnte nicht gespeichert werden')
    }
  }
</script>

<section class="panel">
  <header>
    <h2>Richtungsvalidierung</h2>
    <p>
      Ein kurzer Vorwaertsimpuls prueft die Drehrichtung. Falls das Rad falsch
      herum laeuft, wird der passende Invert-Wert gespeichert.
    </p>
  </header>

  <div class="controls">
    <label>
      Rad
      <select bind:value={selectedWheel} on:change={() => { hasPulseResult = false; lastDecision = '' }}>
        <option value="left">Links</option>
        <option value="right">Rechts</option>
      </select>
    </label>

    <label>
      PWM
      <input type="number" min="0.05" max="1" step="0.01" bind:value={pwm} />
    </label>

    <label>
      Dauer ms
      <input type="number" min="200" max="3000" step="100" bind:value={durationMs} />
    </label>

    <button type="button" class="pulse-button" disabled={$diagnostics.busy} on:click={pulseForward}>
      {$diagnostics.activeAction?.startsWith('direction:') ? 'Teste ...' : 'Vorwaertsimpuls geben'}
    </button>
  </div>

  <div class="actions">
    <button type="button" disabled={$diagnostics.busy || !hasPulseResult} on:click={() => saveDirection(true)}>
      Richtung korrekt
    </button>
    <button type="button" disabled={$diagnostics.busy || !hasPulseResult} on:click={() => saveDirection(false)}>
      Richtung invertieren
    </button>
  </div>

  <div class="note">
    {#if lastDecision}
      <strong>{lastDecision}</strong>
    {:else}
      <span>Nach dem kurzen Vorwaertsimpuls manuell bestaetigen, ob die Richtung stimmt.</span>
    {/if}
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
  .actions {
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

  .controls {
    grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
    align-items: end;
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
    background: #95b8d9;
    color: #07110f;
    font-weight: 700;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .pulse-button {
    align-self: end;
  }

  .note {
    padding: 0.9rem;
    border-radius: 0.9rem;
    background: rgba(24, 38, 34, 0.8);
    border: 1px solid rgba(152, 187, 170, 0.12);
    color: #c8d9d2;
  }
</style>
