<script lang="ts">
  import { runDriveDiag, runMotorDiag, runTurnDiag, setMowMotor } from '../../api/rest'
  import { diagnostics } from '../../stores/diagnostics'
  import { telemetry } from '../../stores/telemetry'

  let pwm = 0.15
  let durationMs = 3000
  let revolutions = 0
  let driveDistanceM = 3
  let drivePwm = 0.15
  let turnPwm = 0.15

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

  async function runDriveDistance() {
    diagnostics.start(`drive:${driveDistanceM.toFixed(2)}m`)
    try {
      const result = await runDriveDiag({
        distance_m: driveDistanceM,
        pwm: drivePwm,
      })
      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Fahrtest fehlgeschlagen')
        return
      }
      diagnostics.success({
        ...result,
        message: `Geradeausfahrt beendet: ${Number(result.distance_m ?? 0).toFixed(2)} m bei L/R ${result.left_ticks ?? 0}/${result.right_ticks ?? 0} Ticks`,
      })
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Fahrtest fehlgeschlagen')
    }
  }

  async function runTurn(angleDeg: number) {
    diagnostics.start(`turn:${angleDeg}deg`)
    try {
      const result = await runTurnDiag({
        angle_deg: angleDeg,
        pwm: turnPwm,
      })
      if (!result.ok) {
        diagnostics.fail(result.error ?? 'Drehtest fehlgeschlagen')
        return
      }
      diagnostics.success({
        ...result,
        message: `Drehtest beendet: ${Number(result.heading_delta_deg ?? 0).toFixed(1)}° bei L/R ${result.left_ticks ?? 0}/${result.right_ticks ?? 0} Ticks`,
      })
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'Drehtest fehlgeschlagen')
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
    <h2>Motor- und Fahrtests</h2>
    <p>Direkte Service-Tests fuer Einzelmotoren, 3-Meter-Fahrt und definierte Drehungen.</p>
  </header>

  <div class="block">
    <div class="block-header">
      <strong>Einzelmotor</strong>
      <span>Radmotoren und Maehmotor separat pruefen.</span>
    </div>

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
  </div>

  <div class="block">
    <div class="block-header">
      <strong>Streckentest</strong>
      <span>Geradeausfahrt ueber Tick-Ziel, um Strecke und Ticks gegenzupruefen.</span>
    </div>

    <div class="settings">
      <label>
        Strecke m
        <input type="number" min="0.5" max="10" step="0.1" bind:value={driveDistanceM} />
      </label>
      <label>
        Test-PWM
        <input type="number" min="0.05" max="1" step="0.01" bind:value={drivePwm} />
      </label>
    </div>

    <div class="actions">
      <button type="button" disabled={$diagnostics.busy} on:click={runDriveDistance}>
        {driveDistanceM.toFixed(1)} m fahren
      </button>
    </div>
  </div>

  <div class="block">
    <div class="block-header">
      <strong>Drehtest</strong>
      <span>In-Place-Drehung fuer 90 / 180 / 360 Grad.</span>
    </div>

    <div class="settings">
      <label>
        Dreh-PWM
        <input type="number" min="0.05" max="1" step="0.01" bind:value={turnPwm} />
      </label>
    </div>

    <div class="actions">
      <button type="button" disabled={$diagnostics.busy} on:click={() => runTurn(90)}>90°</button>
      <button type="button" disabled={$diagnostics.busy} on:click={() => runTurn(180)}>180°</button>
      <button type="button" disabled={$diagnostics.busy} on:click={() => runTurn(360)}>360°</button>
    </div>
  </div>

  <div class="summary">
    <span>Diag-Ticks live: {$telemetry.diag_ticks}</span>
    {#if $diagnostics.lastResult}
      <span>Letztes Ergebnis: {$diagnostics.lastResult.ok ? 'OK' : 'Fehler'}</span>
      {#if $diagnostics.lastResult.distance_target_m !== undefined}
        <span>
          Strecke Soll/Ist:
          {Number($diagnostics.lastResult.distance_target_m ?? 0).toFixed(2)} /
          {Number($diagnostics.lastResult.distance_m ?? 0).toFixed(2)} m
        </span>
      {/if}
      {#if $diagnostics.lastResult.target_angle_deg !== undefined}
        <span>
          Drehung Soll/Ist:
          {Number($diagnostics.lastResult.target_angle_deg ?? 0).toFixed(0)} /
          {Number($diagnostics.lastResult.heading_delta_deg ?? 0).toFixed(1)} °
        </span>
      {/if}
      {#if $diagnostics.lastResult.left_ticks !== undefined || $diagnostics.lastResult.right_ticks !== undefined}
        <span>
          L/R Ticks:
          {$diagnostics.lastResult.left_ticks ?? 0} /
          {$diagnostics.lastResult.right_ticks ?? 0}
        </span>
      {/if}
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
  .block-header {
    display: grid;
    gap: 0.15rem;
  }

  .block {
    display: grid;
    gap: 0.55rem;
    padding: 0.6rem 0.75rem;
    border-radius: 0.55rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
  }

  h2, p { margin: 0; }

  h2 { font-size: 0.92rem; color: #e2e8f0; }

  p,
  .block-header span {
    color: #64748b;
    font-size: 0.78rem;
  }

  .block-header strong { font-size: 0.84rem; color: #cbd5e1; }

  .settings {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(110px, 1fr));
    gap: 0.5rem;
  }

  label {
    display: grid;
    gap: 0.2rem;
    color: #94a3b8;
    font-size: 0.78rem;
  }

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
    display: flex;
    flex-wrap: wrap;
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

  .summary {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem 1rem;
    color: #475569;
    font-size: 0.78rem;
  }
</style>
