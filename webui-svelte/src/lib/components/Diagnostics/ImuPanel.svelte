<script lang="ts">
  import { runImuCalibration } from '../../api/rest'
  import { diagnostics } from '../../stores/diagnostics'
  import { telemetry } from '../../stores/telemetry'

  async function calibrate() {
    diagnostics.start('imu_calib')
    try {
      const result = await runImuCalibration()
      if (!result.ok) {
        diagnostics.fail(result.error ?? 'IMU-Kalibrierung fehlgeschlagen')
        return
      }
      diagnostics.success(result)
    } catch (error) {
      diagnostics.fail(error instanceof Error ? error.message : 'IMU-Kalibrierung fehlgeschlagen')
    }
  }
</script>

<section class="panel">
  <header>
    <h2>IMU</h2>
    <p>Heading, Roll und Pitch sowie ein direkter Kalibrier-Start für V1.</p>
  </header>

  <div class="values">
    <article class="card">
      <span>Heading</span>
      <strong>{$telemetry.imu_h.toFixed(1)} deg</strong>
    </article>
    <article class="card">
      <span>Roll</span>
      <strong>{$telemetry.imu_r.toFixed(1)} deg</strong>
    </article>
    <article class="card">
      <span>Pitch</span>
      <strong>{$telemetry.imu_p.toFixed(1)} deg</strong>
    </article>
  </div>

  <button
    type="button"
    disabled={$diagnostics.busy}
    on:click={calibrate}
  >
    {$diagnostics.activeAction === 'imu_calib' ? 'Kalibriere ...' : 'IMU kalibrieren'}
  </button>
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

  header, .values {
    display: grid;
    gap: 0.45rem;
  }

  h2, p { margin: 0; }

  h2 { font-size: 0.92rem; color: #e2e8f0; }

  p { color: #64748b; font-size: 0.78rem; }

  .values {
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 0.45rem;
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

  button {
    justify-self: start;
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
</style>
