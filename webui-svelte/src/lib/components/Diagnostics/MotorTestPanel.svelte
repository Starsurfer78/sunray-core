<script lang="ts">
  import { runMotorDiag, setMowMotor } from '../../api/rest'
  import { diagnostics } from '../../stores/diagnostics'
  import { telemetry } from '../../stores/telemetry'

  let pwm = 0.15
  let durationMs = 3000
  let revolutions = 0

  async function runMotor(motor: 'left' | 'right' | 'mow') {
    diagnostics.start(`motor:${motor}`)
    try {
      const result = await runMotorDiag({
        motor,
        pwm,
        duration_ms: durationMs,
        revolutions,
      })
      if (!result.ok) {
        diagnostics.fail(result.error ?? `Motortest ${motor} fehlgeschlagen`)
        return
      }
      diagnostics.success(result)
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : `Motortest ${motor} fehlgeschlagen`)
    }
  }

  async function toggleMowMotor(on: boolean) {
    diagnostics.start(on ? 'mow:on' : 'mow:off')
    try {
      const result = await setMowMotor(on)
      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Maehmotor-Test fehlgeschlagen')
        return
      }
      diagnostics.success(result)
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Maehmotor-Test fehlgeschlagen')
    }
  }
</script>

<section class="panel">
  <header>
    <h2>Motor-Test</h2>
    <p>Direkte Tests fuer linkes/rechtes Rad und den Maehmotor als Grundlage fuer die Kalibrierung.</p>
  </header>

  <div class="settings">
    <label>
      PWM
      <input type="number" min="0.05" max="1" step="0.01" bind:value={pwm} />
    </label>
    <label>
      Dauer ms
      <input type="number" min="250" max="15000" step="250" bind:value={durationMs} />
    </label>
    <label>
      Umdrehungen
      <input type="number" min="0" max="10" step="1" bind:value={revolutions} />
    </label>
  </div>

  <div class="actions">
    <button type="button" disabled={$diagnostics.busy} on:click={() => runMotor('left')}>Linkes Rad</button>
    <button type="button" disabled={$diagnostics.busy} on:click={() => runMotor('right')}>Rechtes Rad</button>
    <button type="button" disabled={$diagnostics.busy} on:click={() => runMotor('mow')}>Maehmotor PWM</button>
    <button type="button" disabled={$diagnostics.busy} on:click={() => toggleMowMotor(true)}>Maehmotor EIN</button>
    <button type="button" disabled={$diagnostics.busy} on:click={() => toggleMowMotor(false)}>Maehmotor AUS</button>
  </div>

  <div class="summary">
    <span>Diag-Ticks live: {$telemetry.diag_ticks}</span>
    {#if $diagnostics.lastResult}
      <span>Letztes Ergebnis: {$diagnostics.lastResult.ok ? 'OK' : 'Fehler'}</span>
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

  header {
    display: grid;
    gap: 0.25rem;
  }

  h2, p {
    margin: 0;
  }

  p {
    color: #9db3ab;
  }

  .settings {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 0.8rem;
  }

  label {
    display: grid;
    gap: 0.3rem;
    color: #c8d9d2;
  }

  input {
    width: 100%;
    padding: 0.7rem 0.8rem;
    border: 1px solid rgba(152, 187, 170, 0.2);
    border-radius: 0.8rem;
    background: rgba(24, 38, 34, 0.85);
    color: #dce8e8;
  }

  .actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.8rem;
  }

  button {
    padding: 0.8rem 1rem;
    border: 0;
    border-radius: 0.9rem;
    cursor: pointer;
    background: #9bbf7f;
    color: #07110f;
    font-weight: 700;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }

  .summary {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
    color: #98aca6;
  }
</style>
