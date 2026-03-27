<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'

const { telemetry, connected } = useTelemetry()

// ── Computed ──────────────────────────────────────────────────────────────────

const isIdle  = computed(() => telemetry.value.op === 'Idle')
const canTest = computed(() => connected.value && isIdle.value)

// Heading in degrees (0–360) from IMU if available, else odometry
const headingDeg = computed(() => {
  const h = (telemetry.value.imu_h !== undefined) ? telemetry.value.imu_h : (telemetry.value.heading * 180 / Math.PI)
  return (h % 360 + 360) % 360
})
const headingStr = computed(() => headingDeg.value.toFixed(1) + '°')
// Cardinal direction label
const cardinal = computed(() => {
  const d = headingDeg.value
  if (d < 22.5 || d >= 337.5) return 'N'
  if (d < 67.5)  return 'NO'
  if (d < 112.5) return 'O'
  if (d < 157.5) return 'SO'
  if (d < 202.5) return 'S'
  if (d < 247.5) return 'SW'
  if (d < 292.5) return 'W'
  return 'NW'
})

// ── Toast ─────────────────────────────────────────────────────────────────────

const toast = ref<string | null>(null)
let toastTimer: ReturnType<typeof setTimeout> | null = null

function showToast(msg: string) {
  toast.value = msg
  if (toastTimer) clearTimeout(toastTimer)
  toastTimer = setTimeout(() => { toast.value = null }, 4000)
}

// ── Diag API ──────────────────────────────────────────────────────────────────

async function postDiag(action: string, body: object): Promise<any> {
  const res = await fetch(`/api/diag/${action}`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify(body),
  })
  return res.json()
}

// ── Motor test ────────────────────────────────────────────────────────────────

interface MotorResult {
  ticks: number
  ticks_target: number
  ticks_per_rev_config: number
  deviationPct: number
  error?: string
}

const motorRunning = ref(false)
const motorResults = ref<Record<string, MotorResult | null>>({ left: null, right: null })

async function runMotorTest(motor: 'left' | 'right') {
  if (motorRunning.value) return
  motorRunning.value = true
  motorResults.value[motor] = null
  try {
    // 1 revolution: backend stops when ticks_per_rev ticks accumulated
    const r = await postDiag('motor', { motor, pwm: 0.15, duration_ms: 5000, revolutions: 1 })
    if (r.ok) {
      const cfg = r.ticks_per_rev_config as number
      const measured = r.ticks as number
      const deviationPct = cfg > 0 ? ((measured - cfg) / cfg) * 100 : 0
      motorResults.value[motor] = { ticks: measured, ticks_target: r.ticks_target, ticks_per_rev_config: cfg, deviationPct }
      showToast(`Motor ${motor === 'left' ? 'Links' : 'Rechts'}: ${measured} Ticks (${deviationPct >= 0 ? '+' : ''}${deviationPct.toFixed(1)}%)`)
    } else {
      showToast(`Fehler: ${r.error}`)
    }
  } catch {
    showToast('Verbindungsfehler')
  } finally {
    motorRunning.value = false
  }
}

// ── Mow motor ─────────────────────────────────────────────────────────────────

const mowOn      = ref(false)
const mowPending = ref(false)
const mowConfirm = ref(false)

function requestMowToggle() {
  if (!mowOn.value) { mowConfirm.value = true }
  else { doMowToggle(false) }
}

async function doMowToggle(on: boolean) {
  mowConfirm.value = false
  mowPending.value = true
  try {
    const r = await postDiag('mow', { on })
    if (r.ok) {
      mowOn.value = on
      showToast(on ? 'Mähmotor eingeschaltet' : 'Mähmotor ausgeschaltet')
    } else {
      showToast(`Fehler: ${r.error}`)
    }
  } catch {
    showToast('Verbindungsfehler')
  } finally {
    mowPending.value = false
  }
}

// ── Drive test ────────────────────────────────────────────────────────────────

const driveRunning = ref(false)
const driveResult  = ref<string | null>(null)

async function runDriveTest() {
  if (driveRunning.value) return
  driveRunning.value = true
  driveResult.value  = null
  try {
    const r = await postDiag('drive', { distance_m: 3.0 })
    if (r.ok) {
      driveResult.value = `Ticks: L=${r.left} R=${r.right}`
      showToast(`Fahrt abgeschlossen: L=${r.left} R=${r.right}`)
    } else {
      showToast(`Fehler: ${r.error}`)
    }
  } catch {
    showToast('Verbindungsfehler')
  } finally {
    driveRunning.value = false
  }
}

// ── IMU calibration ───────────────────────────────────────────────────────────

const imuCalibrating = ref(false)

async function runImuCalib() {
  if (imuCalibrating.value) return
  imuCalibrating.value = true
  try {
    showToast('IMU-Kalibrierung gestartet... Roboter stillhalten (5s)')
    const r = await postDiag('imu_calib', {})
    if (r.ok) {
      showToast('IMU-Kalibrierung erfolgreich')
    } else {
      showToast(`Fehler: ${r.error}`)
    }
  } catch {
    showToast('Verbindungsfehler')
  } finally {
    imuCalibrating.value = false
  }
}

// ── Rad-Parameter ─────────────────────────────────────────────────────────────

interface RadCfg { ticks_per_rev: number; wheel_diameter_m: number; wheel_base_m: number }

const radCfg     = ref<RadCfg>({ ticks_per_rev: 1060, wheel_diameter_m: 0.165, wheel_base_m: 0.325 })
const radCfgOrig = ref<RadCfg>({ ...radCfg.value })
const radSaving  = ref(false)
const radDirty   = computed(() => JSON.stringify(radCfg.value) !== JSON.stringify(radCfgOrig.value))

async function loadRadConfig() {
  try {
    const res = await fetch('/api/config')
    if (!res.ok) return
    const j = await res.json()
    if (j.ticks_per_rev    !== undefined) radCfg.value.ticks_per_rev    = j.ticks_per_rev
    if (j.wheel_diameter_m !== undefined) radCfg.value.wheel_diameter_m = j.wheel_diameter_m
    if (j.wheel_base_m     !== undefined) radCfg.value.wheel_base_m     = j.wheel_base_m
    radCfgOrig.value = { ...radCfg.value }
  } catch {}
}

async function saveRadConfig() {
  radSaving.value = true
  try {
    const res = await fetch('/api/config', {
      method:  'PUT',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify({ ...radCfg.value }),
    })
    if (res.ok) {
      radCfgOrig.value = { ...radCfg.value }
      showToast('Rad-Parameter gespeichert')
    } else {
      showToast('Speichern fehlgeschlagen')
    }
  } catch {
    showToast('Verbindungsfehler')
  } finally {
    radSaving.value = false
  }
}

onMounted(loadRadConfig)
</script>

<template>
  <div class="dg-wrap">
    <h2 class="dg-title">Diagnose</h2>

    <!-- Toast -->
    <Transition name="toast">
      <div v-if="toast" class="dg-toast">{{ toast }}</div>
    </Transition>

    <!-- Mow-Motor confirmation modal -->
    <Transition name="modal">
      <div v-if="mowConfirm" class="dg-overlay">
        <div class="dg-modal">
          <div class="dg-modal-icon">⚠</div>
          <p class="dg-modal-txt">
            Mähmotor einschalten?<br>
            <span class="dg-modal-sub">Klingen drehen sich — Verletzungsgefahr!</span>
          </p>
          <div class="dg-modal-btns">
            <button class="dg-btn dg-btn-danger" @click="doMowToggle(true)">Einschalten</button>
            <button class="dg-btn dg-btn-secondary" @click="mowConfirm = false">Abbrechen</button>
          </div>
        </div>
      </div>
    </Transition>

    <!-- Idle warning -->
    <div v-if="!isIdle && connected" class="dg-warn">
      Tests nur im Idle-Zustand verfügbar — aktuell: <strong>{{ telemetry.op }}</strong>
    </div>

    <!-- ── Two-column grid ──────────────────────────────────────────────── -->
    <div class="dg-grid">

      <!-- ── LEFT COLUMN ────────────────────────────────────────────────── -->
      <div class="dg-col">

        <!-- Sensor Live-Anzeige -->
        <div class="dg-card">
          <div class="dg-card-hdr">Sensor-Status (live)</div>
          <div class="dg-sensors">
            <div class="dg-sensor" :class="{ active: telemetry.bumper_l }">
              <div class="dg-sensor-led" :class="{ on: telemetry.bumper_l }" />
              <span>Bumper Links</span>
            </div>
            <div class="dg-sensor" :class="{ active: telemetry.bumper_r }">
              <div class="dg-sensor-led" :class="{ on: telemetry.bumper_r }" />
              <span>Bumper Rechts</span>
            </div>
            <div class="dg-sensor" :class="{ active: telemetry.lift }">
              <div class="dg-sensor-led" :class="{ on: telemetry.lift }" />
              <span>Lift</span>
            </div>
            <div class="dg-sensor" :class="{ active: telemetry.motor_err }">
              <div class="dg-sensor-led dg-sensor-led-red" :class="{ on: telemetry.motor_err }" />
              <span>Motor-Fehler</span>
            </div>
          </div>
        </div>

        <!-- Motor-Test -->
        <div class="dg-card">
          <div class="dg-card-hdr">Motor-Test — 1 Radumdrehung</div>
          <p class="dg-hint">Motor dreht genau 1 Umdrehung (tick-basiert). Räder drehen sich!</p>
          <div class="dg-motor-rows">

            <div class="dg-motor-row">
              <button class="dg-btn dg-btn-test"
                :disabled="!canTest || motorRunning"
                @click="runMotorTest('left')">
                {{ motorRunning ? '…läuft' : 'Motor Links — 1 Umdrehung' }}
              </button>
              <span v-if="motorRunning && telemetry.diag_active" class="dg-live-ticks">
                {{ telemetry.diag_ticks }} Ticks
              </span>
            </div>
            <div v-if="motorResults.left" class="dg-rev-result">
              <span class="dg-rev-label">Gemessen</span>
              <span class="dg-rev-val">{{ motorResults.left.ticks }} Ticks</span>
              <span class="dg-rev-label">Erwartet</span>
              <span class="dg-rev-val">{{ motorResults.left.ticks_per_rev_config }} Ticks</span>
              <span class="dg-rev-label">Abw.</span>
              <span class="dg-rev-dev"
                :class="Math.abs(motorResults.left.deviationPct) > 5 ? 'dg-rev-warn' : 'dg-rev-ok'">
                {{ motorResults.left.deviationPct >= 0 ? '+' : '' }}{{ motorResults.left.deviationPct.toFixed(1) }}%
              </span>
            </div>

            <div class="dg-motor-row">
              <button class="dg-btn dg-btn-test"
                :disabled="!canTest || motorRunning"
                @click="runMotorTest('right')">
                {{ motorRunning ? '…läuft' : 'Motor Rechts — 1 Umdrehung' }}
              </button>
              <span v-if="motorRunning && telemetry.diag_active" class="dg-live-ticks">
                {{ telemetry.diag_ticks }} Ticks
              </span>
            </div>
            <div v-if="motorResults.right" class="dg-rev-result">
              <span class="dg-rev-label">Gemessen</span>
              <span class="dg-rev-val">{{ motorResults.right.ticks }} Ticks</span>
              <span class="dg-rev-label">Erwartet</span>
              <span class="dg-rev-val">{{ motorResults.right.ticks_per_rev_config }} Ticks</span>
              <span class="dg-rev-label">Abw.</span>
              <span class="dg-rev-dev"
                :class="Math.abs(motorResults.right.deviationPct) > 5 ? 'dg-rev-warn' : 'dg-rev-ok'">
                {{ motorResults.right.deviationPct >= 0 ? '+' : '' }}{{ motorResults.right.deviationPct.toFixed(1) }}%
              </span>
            </div>

            <div class="dg-motor-row">
              <button class="dg-btn"
                :class="mowOn ? 'dg-btn-danger' : 'dg-btn-warn'"
                :disabled="!canTest || mowPending"
                @click="requestMowToggle()">
                {{ mowPending ? '…' : mowOn ? 'Mähmotor AUS' : 'Mähmotor EIN' }}
              </button>
              <span v-if="mowOn" class="dg-mow-on-badge">LÄUFT</span>
            </div>

            <div class="dg-motor-row">
              <button class="dg-btn dg-btn-test"
                :disabled="!canTest || driveRunning"
                @click="runDriveTest()">
                {{ driveRunning ? '…fährt' : 'Geradeausfahrt 3 m' }}
              </button>
              <div v-if="driveResult" class="dg-result">{{ driveResult }}</div>
            </div>

          </div>
        </div>

      </div><!-- /LEFT -->

      <!-- ── RIGHT COLUMN ───────────────────────────────────────────────── -->
      <div class="dg-col">

        <!-- Rad-Parameter -->
        <div class="dg-card">
          <div class="dg-card-hdr">
            Rad-Parameter
            <span v-if="radDirty" class="dg-badge-dirty">Ungespeichert</span>
          </div>
          <div class="dg-rad-fields">
            <div class="dg-rad-row">
              <label class="dg-rad-lbl">Ticks / Umdrehung</label>
              <input v-model.number="radCfg.ticks_per_rev" type="number"
                class="dg-rad-input" :class="{ dirty: radCfg.ticks_per_rev !== radCfgOrig.ticks_per_rev }"
                step="1" min="100" max="9999" />
              <span class="dg-rad-unit">Ticks</span>
            </div>
            <div class="dg-rad-row">
              <label class="dg-rad-lbl">Raddurchmesser</label>
              <input v-model.number="radCfg.wheel_diameter_m" type="number"
                class="dg-rad-input" :class="{ dirty: radCfg.wheel_diameter_m !== radCfgOrig.wheel_diameter_m }"
                step="0.001" min="0.05" max="0.5" />
              <span class="dg-rad-unit">m</span>
            </div>
            <div class="dg-rad-row">
              <label class="dg-rad-lbl">Radabstand</label>
              <input v-model.number="radCfg.wheel_base_m" type="number"
                class="dg-rad-input" :class="{ dirty: radCfg.wheel_base_m !== radCfgOrig.wheel_base_m }"
                step="0.001" min="0.1" max="1.0" />
              <span class="dg-rad-unit">m</span>
            </div>
          </div>
          <div class="dg-rad-footer">
            <span class="dg-rad-hint">ticks_per_rev · wheel_diameter_m · wheel_base_m</span>
            <button class="dg-btn dg-btn-save"
              :disabled="!radDirty || radSaving"
              @click="saveRadConfig">
              {{ radSaving ? 'Speichert…' : '✓ Speichern' }}
            </button>
          </div>
        </div>

        <!-- IMU-Kalibrierung -->
        <div class="dg-card">
          <div class="dg-card-hdr">
            IMU-Kalibrierung
            <span v-if="telemetry.imu_h !== undefined" class="dg-badge-ok">Aktiv</span>
          </div>
          <p class="dg-hint">
            MPU6050 per I2C am Pi.<br>
            Gyro-Bias-Korrektur: Roboter absolut stillhalten (ca. 5 s).
          </p>
          <button 
            class="dg-btn" 
            :class="imuCalibrating ? 'dg-btn-off' : 'dg-btn-test'"
            :disabled="!canTest || imuCalibrating"
            @click="runImuCalib">
            {{ imuCalibrating ? 'Kalibriere…' : 'Kalibrierung starten' }}
          </button>
        </div>

        <!-- IMU / Kompassrose -->
        <div class="dg-card">
          <div class="dg-card-hdr">
            Heading
            <span :class="telemetry.ekf_health === 'EKF+GPS' ? 'dg-badge-ok' : telemetry.ekf_health === 'EKF+IMU' ? 'dg-badge-warn' : 'dg-badge-na'">
              {{ telemetry.ekf_health || 'Odo' }}
            </span>
          </div>

          <!-- Compass rose SVG -->
          <div class="dg-compass-wrap">
            <svg viewBox="0 0 120 120" class="dg-compass" aria-label="Kompassrose">
              <!-- Background ring -->
              <circle cx="60" cy="60" r="54" class="dg-c-ring" />
              <!-- Tick marks (every 30°) -->
              <g v-for="i in 12" :key="i">
                <line
                  :x1="60 + 48 * Math.sin((i - 1) * 30 * Math.PI / 180)"
                  :y1="60 - 48 * Math.cos((i - 1) * 30 * Math.PI / 180)"
                  :x2="60 + 54 * Math.sin((i - 1) * 30 * Math.PI / 180)"
                  :y2="60 - 54 * Math.cos((i - 1) * 30 * Math.PI / 180)"
                  class="dg-c-tick"
                />
              </g>
              <!-- Cardinal labels -->
              <text x="60" y="11"  class="dg-c-label dg-c-north">N</text>
              <text x="109" y="65" class="dg-c-label">O</text>
              <text x="60" y="116" class="dg-c-label">S</text>
              <text x="11" y="65"  class="dg-c-label">W</text>
              <!-- Needle group — rotates by heading -->
              <g :transform="`rotate(${headingDeg}, 60, 60)`">
                <!-- North half (red) -->
                <polygon points="60,12 56,60 64,60" class="dg-c-needle-n" />
                <!-- South half (dark) -->
                <polygon points="60,108 56,60 64,60" class="dg-c-needle-s" />
                <!-- Center cap -->
                <circle cx="60" cy="60" r="5" class="dg-c-center" />
              </g>
            </svg>

            <!-- Readout -->
            <div class="dg-compass-info">
              <div class="dg-compass-deg">{{ headingStr }}</div>
              <div class="dg-compass-card">{{ cardinal }}</div>
            </div>
          </div>
        </div>

      </div><!-- /RIGHT -->

    </div><!-- /grid -->

    <!-- ── Telemetrie-Rohdaten (full width) ───────────────────────────── -->
    <div class="dg-card">
      <div class="dg-card-hdr">Telemetrie-Rohdaten</div>
      <pre class="dg-raw">{{ JSON.stringify(telemetry, null, 2) }}</pre>
    </div>

  </div>
</template>

<style scoped>
.dg-wrap {
  padding: 16px; display: flex; flex-direction: column; gap: 14px;
  font-family: sans-serif; background: #0a0f1a !important; min-height: 100%;
  position: relative;
}
.dg-title { font-size: 15px; font-weight: 600; color: #e2e8f0 !important; margin: 0; }

/* Toast */
.dg-toast {
  position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
  background: #1e3a5f; border: 1px solid #3b82f6; border-radius: 8px;
  color: #93c5fd !important; padding: 10px 18px; font-size: 13px; z-index: 100;
  pointer-events: none; white-space: nowrap;
}
.toast-enter-active, .toast-leave-active { transition: opacity 0.3s, transform 0.3s; }
.toast-enter-from, .toast-leave-to { opacity: 0; transform: translateX(-50%) translateY(10px); }

/* Modal overlay */
.dg-overlay {
  position: fixed; inset: 0; background: rgba(0,0,0,0.7);
  display: flex; align-items: center; justify-content: center; z-index: 200;
}
.dg-modal {
  background: #0f1829 !important; border: 1px solid #7f1d1d; border-radius: 12px;
  padding: 24px; text-align: center; max-width: 320px;
}
.dg-modal-icon { font-size: 32px; margin-bottom: 10px; color: #ef4444 !important; }
.dg-modal-txt  { color: #e2e8f0 !important; font-size: 14px; margin: 0 0 16px; }
.dg-modal-sub  { font-size: 12px; color: #ef4444 !important; }
.dg-modal-btns { display: flex; gap: 10px; justify-content: center; }
.modal-enter-active, .modal-leave-active { transition: opacity 0.2s; }
.modal-enter-from, .modal-leave-to { opacity: 0; }

/* Two-column grid */
.dg-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 14px;
  align-items: start;
}
@media (max-width: 600px) {
  .dg-grid { grid-template-columns: 1fr; }
}
.dg-col { display: flex; flex-direction: column; gap: 14px; }

/* Cards */
.dg-card {
  background: #0f1829 !important; border: 1px solid #1e3a5f; border-radius: 10px; padding: 14px;
}
.dg-card-disabled { opacity: 0.5; }
.dg-card-hdr {
  font-size: 11px; color: #4b6a8a !important; text-transform: uppercase;
  letter-spacing: 0.06em; margin-bottom: 12px; display: flex; align-items: center; gap: 8px;
}
.dg-badge-na {
  font-size: 10px; background: #1e2d3d; border: 1px solid #334155;
  color: #64748b !important; border-radius: 4px; padding: 1px 6px; text-transform: none;
}
.dg-badge-ok {
  font-size: 10px; background: #052e16; border: 1px solid #16a34a;
  color: #4ade80 !important; border-radius: 4px; padding: 1px 6px; text-transform: none;
}
.dg-badge-warn {
  font-size: 10px; background: #1c1917; border: 1px solid #d97706;
  color: #fbbf24 !important; border-radius: 4px; padding: 1px 6px; text-transform: none;
}
.dg-badge-dirty {
  font-size: 10px; background: #1c1200; border: 1px solid #d97706;
  color: #fbbf24 !important; border-radius: 4px; padding: 1px 6px; text-transform: none;
}
.dg-hint { font-size: 12px; color: #f59e0b !important; margin: 0 0 10px; }
.dg-hint-disabled { font-size: 12px; color: #4b6a8a !important; margin: 0 0 10px; }
.dg-warn {
  background: #1c1200; border: 1px solid #78350f; border-radius: 8px;
  padding: 10px 14px; font-size: 12px; color: #fcd34d !important;
}

/* Sensor LEDs */
.dg-sensors { display: flex; flex-wrap: wrap; gap: 8px; }
.dg-sensor {
  display: flex; align-items: center; gap: 8px;
  background: #070d18; border: 1px solid #1e2d3d; border-radius: 8px;
  padding: 7px 10px; font-size: 12px; color: #64748b !important;
  transition: border-color 0.2s, color 0.2s;
}
.dg-sensor.active { border-color: #22c55e; color: #86efac !important; }
.dg-sensor-led {
  width: 10px; height: 10px; border-radius: 50%;
  background: #1e2d3d; transition: background 0.15s, box-shadow 0.15s; flex-shrink: 0;
}
.dg-sensor-led.on { background: #22c55e; box-shadow: 0 0 6px #22c55e; }
.dg-sensor-led-red.on { background: #ef4444; box-shadow: 0 0 6px #ef4444; }

/* Buttons */
.dg-btn {
  padding: 8px 14px; font-size: 12px; font-weight: 600; border-radius: 7px;
  border: 1px solid; cursor: pointer; font-family: sans-serif;
  transition: opacity 0.15s, transform 0.1s;
}
.dg-btn:disabled { opacity: 0.35; cursor: not-allowed; }
.dg-btn:not(:disabled):active { transform: scale(0.96); }

.dg-btn-test      { background: #0f2744 !important; border-color: #3b82f6; color: #93c5fd !important; }
.dg-btn-warn      { background: #451a03 !important; border-color: #f59e0b; color: #fcd34d !important; }
.dg-btn-danger    { background: #450a0a !important; border-color: #ef4444; color: #fca5a5 !important; }
.dg-btn-secondary { background: #1e2d3d !important; border-color: #334155; color: #94a3b8 !important; }
.dg-btn-off       { background: #0f1829 !important; border-color: #1e2d3d; color: #334155 !important; }
.dg-btn-save      { background: #0c1a3a !important; border-color: #2563eb; color: #93c5fd !important; font-size: 12px; }
.dg-btn-save:not(:disabled):hover { background: #112455 !important; }

/* Motor rows */
.dg-motor-rows { display: flex; flex-direction: column; gap: 10px; }
.dg-motor-row  { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
.dg-result     { font-size: 12px; color: #60a5fa !important; }
.dg-result-val { font-weight: 700; font-size: 14px; }
.dg-result-cfg { color: #4b6a8a !important; margin-left: 4px; }

/* Live tick counter (during test) */
.dg-live-ticks {
  font-size: 13px; font-weight: 700; font-family: monospace;
  color: #f59e0b !important;
  background: #1c1200; border: 1px solid #78350f; border-radius: 5px;
  padding: 3px 10px; animation: blink 0.8s infinite alternate;
}

/* Revolution result grid */
.dg-rev-result {
  display: grid; grid-template-columns: auto 1fr auto 1fr auto 1fr;
  align-items: center; gap: 6px 8px;
  background: #070d18; border: 1px solid #1e2d3d; border-radius: 7px;
  padding: 8px 12px; font-size: 12px;
}
.dg-rev-label { color: #4b6a8a !important; font-size: 10px; text-transform: uppercase; letter-spacing: 0.05em; }
.dg-rev-val   { color: #60a5fa !important; font-family: monospace; font-weight: 600; }
.dg-rev-dev   { font-family: monospace; font-weight: 700; }
.dg-rev-ok    { color: #22c55e !important; }
.dg-rev-warn  { color: #ef4444 !important; }
.dg-mow-on-badge {
  font-size: 11px; font-weight: 700; color: #fca5a5 !important;
  background: #450a0a; border: 1px solid #ef4444; border-radius: 4px;
  padding: 2px 8px; animation: blink 1s infinite;
}
@keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.4} }

/* Rad-Parameter */
.dg-rad-fields { display: flex; flex-direction: column; gap: 8px; margin-bottom: 12px; }
.dg-rad-row { display: flex; align-items: center; gap: 8px; }
.dg-rad-lbl { flex: 1; font-size: 12px; color: #94a3b8 !important; }
.dg-rad-input {
  width: 80px; background: #070d18 !important; border: 1px solid #1e3a5f;
  color: #e2e8f0 !important; font-size: 12px; font-family: monospace;
  border-radius: 5px; padding: 5px 8px; outline: none; text-align: right;
}
.dg-rad-input:focus { border-color: #2563eb; }
.dg-rad-input.dirty { border-color: #d97706 !important; color: #fbbf24 !important; }
.dg-rad-unit { font-size: 11px; color: #475569 !important; width: 28px; flex-shrink: 0; }
.dg-rad-footer {
  display: flex; align-items: center; justify-content: space-between;
  padding-top: 10px; border-top: 1px solid #1e2d3d; gap: 8px;
}
.dg-rad-hint { font-size: 10px; color: #334155 !important; font-family: monospace; }

/* Compass rose */
.dg-compass-wrap {
  display: flex; align-items: center; gap: 20px; justify-content: center; padding: 8px 0;
}
.dg-compass { width: 120px; height: 120px; flex-shrink: 0; }

.dg-c-ring  { fill: #070d18; stroke: #1e3a5f; stroke-width: 1.5; }
.dg-c-tick  { stroke: #1e3a5f; stroke-width: 1; }
.dg-c-label {
  fill: #4b6a8a; font-size: 11px; font-family: sans-serif; font-weight: 600;
  text-anchor: middle; dominant-baseline: central;
}
.dg-c-north { fill: #ef4444; }
.dg-c-needle-n { fill: #ef4444; opacity: 0.9; }
.dg-c-needle-s { fill: #334155; opacity: 0.7; }
.dg-c-center    { fill: #1e3a5f; stroke: #60a5fa; stroke-width: 1; }

.dg-compass-info { display: flex; flex-direction: column; align-items: center; gap: 4px; }
.dg-compass-deg  {
  font-size: 22px; font-weight: 700; font-family: monospace;
  color: #60a5fa !important; line-height: 1;
}
.dg-compass-card {
  font-size: 13px; font-weight: 600; color: #4b6a8a !important; letter-spacing: 0.05em;
}

/* Raw data */
.dg-raw {
  font-size: 11px; color: #22c55e !important; background: #070d18 !important;
  border-radius: 6px; padding: 10px; overflow: auto; max-height: 200px; margin: 0;
}
</style>
