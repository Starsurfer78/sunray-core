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
    <p>Heading, Roll und Pitch sowie ein direkter Kalibrier-Start fuer V1.</p>
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
    gap: 1rem;
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header, .values {
    display: grid;
    gap: 0.8rem;
  }

  h2, p {
    margin: 0;
  }

  p {
    color: #9db3ab;
  }

  .values {
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

  button {
    justify-self: start;
    padding: 0.8rem 1rem;
    border: 0;
    border-radius: 0.9rem;
    cursor: pointer;
    background: #86c6ac;
    color: #07110f;
    font-weight: 700;
  }

  button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
</style>
