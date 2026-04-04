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
        diagnostics.fail(result.error ?? 'Richtungsprüfung fehlgeschlagen')
        return
      }

      hasPulseResult = true
      diagnostics.success(result)
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Richtungsprüfung fehlgeschlagen')
    }
  }

  async function saveDirection(isCorrectForward: boolean) {
    if (!hasPulseResult) {
      diagnostics.fail('Erst einen Vorwärtsimpuls ausführen und die Richtung beobachten')
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
        ? `${selectedWheel} vorwärts ist korrekt`
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
      Ein kurzer Vorwärtsimpuls prüft die Drehrichtung. Falls das Rad falsch
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
      {$diagnostics.activeAction?.startsWith('direction:') ? 'Teste ...' : 'Vorwärtsimpuls geben'}
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
      <span>Nach dem kurzen Vorwärtsimpuls manuell bestätigen, ob die Richtung stimmt.</span>
    {/if}
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
  .actions {
    display: grid;
    gap: 0.45rem;
  }

  h2, p { margin: 0; }

  h2 { font-size: 0.92rem; color: #e2e8f0; }

  p { color: #64748b; font-size: 0.78rem; }

  .controls {
    grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
    align-items: end;
    gap: 0.5rem;
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
    grid-template-columns: repeat(auto-fit, minmax(150px, max-content));
    gap: 0.4rem;
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

  .pulse-button {
    align-self: end;
  }

  .note {
    padding: 0.55rem 0.75rem;
    border-radius: 0.55rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
    color: #94a3b8;
    font-size: 0.82rem;
  }
</style>
