<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useTelemetry } from '../composables/useTelemetry'

const router = useRouter()
const { telemetry, connected, logs, clearLogs } = useTelemetry()

// ── Uptime ────────────────────────────────────────────────────────────────────
const uptime = computed(() => {
  const s = telemetry.value.uptime_s
  const h = Math.floor(s / 3600)
  const m = Math.floor((s % 3600) / 60)
  return `${h}h ${String(m).padStart(2, '0')}m`
})

// ── System info (Sektion 1) ───────────────────────────────────────────────────
const missionVersion = ref('—')
const mapPath        = ref('/etc/sunray/map.json')

async function fetchVersion() {
  try {
    const res = await fetch('/api/version')
    if (res.ok) {
      const j = await res.json()
      missionVersion.value = j.version ?? JSON.stringify(j)
    }
  } catch { /* backend may not have /api/version */ }
}

// ── Sektion 2: Roboter-Konfiguration ─────────────────────────────────────────

interface FieldDef {
  key:   string
  label: string
  unit:  string
  step:  number
  min:   number
  max:   number
}

const FIELDS: FieldDef[] = [
  { key: 'strip_width_default',       label: 'Schnittbreite Standard', unit: 'm',   step: 0.01, min: 0.05, max: 0.5  },
  { key: 'speed_default',             label: 'Fahrgeschwindigkeit',    unit: 'm/s', step: 0.01, min: 0.01, max: 0.3  },
  { key: 'battery_low_v',             label: 'Akku Warnschwelle',      unit: 'V',   step: 0.1,  min: 18.0, max: 28.0 },
  { key: 'battery_critical_v',        label: 'Akku Kritisch',          unit: 'V',   step: 0.1,  min: 16.0, max: 26.0 },
  { key: 'gps_no_motion_threshold_m', label: 'GPS No-Motion',          unit: 'm',   step: 0.01, min: 0.01, max: 0.5  },
  { key: 'rain_delay_min',            label: 'Regen-Verzögerung',      unit: 'min', step: 5,    min: 0,    max: 240  },
]

const FALLBACKS: Record<string, number> = {
  strip_width_default:       0.25,
  speed_default:             0.3,
  battery_low_v:             22.0,
  battery_critical_v:        20.0,
  gps_no_motion_threshold_m: 0.05,
  rain_delay_min:            60,
}

const originals  = ref<Record<string, number>>({})
const values     = ref<Record<string, number>>({})
const cfgLoading = ref(false)
const cfgSaving  = ref(false)

const isDirty = computed(() =>
  FIELDS.some(f => values.value[f.key] !== originals.value[f.key])
)

async function loadConfig() {
  cfgLoading.value = true
  try {
    const res = await fetch('/api/config')
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    const json = await res.json()

    const loaded: Record<string, number> = {}
    for (const f of FIELDS) {
      loaded[f.key] = typeof json[f.key] === 'number' ? json[f.key] : FALLBACKS[f.key]
    }
    originals.value = { ...loaded }
    values.value    = { ...loaded }

    if (typeof json['map_path'] === 'string') mapPath.value = json['map_path']
  } catch (e) {
    showToast(`Config laden: ${e}`, 'error')
  } finally {
    cfgLoading.value = false
  }
}

async function saveConfig() {
  const changed: Record<string, number> = {}
  for (const f of FIELDS) {
    if (values.value[f.key] !== originals.value[f.key]) {
      changed[f.key] = values.value[f.key]
    }
  }
  if (Object.keys(changed).length === 0) return

  cfgSaving.value = true
  try {
    const res = await fetch('/api/config', {
      method:  'PUT',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(changed),
    })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    originals.value = { ...values.value }
    showToast('Gespeichert', 'success')
  } catch (e) {
    showToast(`Speichern fehlgeschlagen: ${e}`, 'error')
  } finally {
    cfgSaving.value = false
  }
}

// ── Sektion 3: Diagnose ───────────────────────────────────────────────────────

const motorBusy = ref(false)
let   motorResetTimer: ReturnType<typeof setTimeout> | null = null

async function motorTest(side: string) {
  if (motorBusy.value) return
  motorBusy.value = true
  try {
    const res = await fetch('/api/sim/motor', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify({ side, speed: 0.5, duration_ms: 2000 }),
    })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    showToast(`Motor ${side}: läuft 2 s`, 'success')
  } catch (e) {
    showToast(`Motor-Test: ${e}`, 'error')
  }
  if (motorResetTimer) clearTimeout(motorResetTimer)
  motorResetTimer = setTimeout(() => { motorBusy.value = false }, 2100)
}

const imuState = ref<'idle' | 'running' | 'done'>('idle')

async function imuCalib() {
  imuState.value = 'running'
  try {
    const res = await fetch('/api/sim/imu_calib', { method: 'POST' })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    setTimeout(() => { imuState.value = 'done' }, 3000)
  } catch (e) {
    imuState.value = 'idle'
    showToast(`IMU-Kalibrierung: ${e}`, 'error')
  }
}

// ── Toast ─────────────────────────────────────────────────────────────────────
const toast      = ref<{ text: string; type: 'success' | 'error' } | null>(null)
let   toastTimer: ReturnType<typeof setTimeout> | null = null

function showToast(text: string, type: 'success' | 'error') {
  if (toastTimer) clearTimeout(toastTimer)
  toast.value = { text, type }
  toastTimer  = setTimeout(() => { toast.value = null }, 2000)
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
onMounted(() => {
  loadConfig()
  fetchVersion()
})
</script>

<template>
  <div class="st-page">

    <!-- ════════════════════════════════════════════
         SEKTION 1 — System
         ════════════════════════════════════════════ -->
    <div class="st-card">
      <div class="st-head">System</div>

      <div class="st-rows">
        <div class="st-row">
          <span class="st-key">Mission Service</span>
          <span class="st-val">{{ missionVersion }}</span>
        </div>
        <div class="st-row">
          <span class="st-key">Sunray Socket</span>
          <span class="st-val" :class="connected ? 'st-green' : 'st-red'">
            <span class="st-dot" :class="connected ? 'st-dot-green' : 'st-dot-red'"></span>
            {{ connected ? 'Verbunden' : 'Getrennt' }}
          </span>
        </div>
        <div class="st-row">
          <span class="st-key">Uptime</span>
          <span class="st-val st-mono">{{ uptime }}</span>
        </div>
        <div class="st-row">
          <span class="st-key">GPS</span>
          <span class="st-val st-mono">{{ telemetry.gps_text }}</span>
        </div>
        <div class="st-row">
          <span class="st-key">Aktive Karte</span>
          <span class="st-val">
            <span class="st-mono st-muted">{{ mapPath }}</span>
            <a class="st-link" @click.prevent="router.push('/map')" href="#">
              Karte öffnen →
            </a>
          </span>
        </div>
        <div class="st-row">
          <span class="st-key">API-Docs</span>
          <span class="st-val">
            <a class="st-link" href="/docs"    target="_blank">Swagger /docs</a>
            <span class="st-sep">·</span>
            <a class="st-link" href="/redoc"   target="_blank">ReDoc /redoc</a>
          </span>
        </div>
      </div>
    </div>

    <!-- ════════════════════════════════════════════
         SEKTION 2 — Roboter-Konfiguration
         ════════════════════════════════════════════ -->
    <div class="st-card">
      <div class="st-head">
        Roboter-Konfiguration
        <span v-if="cfgLoading" class="st-badge st-badge-muted">Lade…</span>
        <span v-if="isDirty"    class="st-badge st-badge-amber">Ungespeicherte Änderungen</span>
      </div>

      <div class="st-fields">
        <div v-for="f in FIELDS" :key="f.key" class="st-field">
          <label class="st-flbl">{{ f.label }}</label>
          <div class="st-finput-wrap">
            <input
              v-model.number="values[f.key]"
              type="number"
              class="st-finput"
              :step="f.step"
              :min="f.min"
              :max="f.max"
              :class="{ 'st-finput-changed': values[f.key] !== originals[f.key] }"
            />
            <span class="st-unit">{{ f.unit }}</span>
          </div>
          <span class="st-fkey">{{ f.key }}</span>
        </div>
      </div>

      <div class="st-cfooter">
        <span class="st-hint">/etc/sunray/config.json — Änderungen sofort aktiv</span>
        <div class="st-btnrow">
          <button class="st-btn st-btn-ghost" @click="loadConfig" :disabled="cfgLoading">
            ↺ Neu laden
          </button>
          <button
            class="st-btn st-btn-save"
            :disabled="!isDirty || cfgSaving"
            @click="saveConfig"
          >
            {{ cfgSaving ? 'Speichert…' : '✓ Speichern' }}
          </button>
        </div>
      </div>
    </div>

    <!-- ════════════════════════════════════════════
         SEKTION 3 — Diagnose
         ════════════════════════════════════════════ -->
    <div class="st-card">
      <div class="st-head">Diagnose</div>

      <!-- Motor-Tests -->
      <div class="st-subsec">
        <div class="st-sublbl">Motor-Tests</div>
        <div class="st-warn">
          ⚠ Nur im Stillstand ausführen
        </div>
        <div class="st-motorrow">
          <button
            class="st-btn st-btn-motor"
            :disabled="!connected || motorBusy"
            @click="motorTest('left')"
          >Motor Links</button>
          <button
            class="st-btn st-btn-motor"
            :disabled="!connected || motorBusy"
            @click="motorTest('right')"
          >Motor Rechts</button>
          <button
            class="st-btn st-btn-motor"
            :disabled="!connected || motorBusy"
            @click="motorTest('mow')"
          >Mähwerk</button>
        </div>
      </div>

      <!-- IMU-Kalibrierung -->
      <div class="st-subsec">
        <div class="st-sublbl">IMU-Kalibrierung</div>
        <div class="st-imurow">
          <button
            class="st-btn st-btn-imu"
            :disabled="!connected || imuState === 'running'"
            @click="imuCalib"
          >Kalibrierung starten</button>
          <span
            class="st-imustatus"
            :class="{
              'st-imu-running': imuState === 'running',
              'st-imu-done':    imuState === 'done',
              'st-imu-idle':    imuState === 'idle',
            }"
          >
            {{ imuState === 'idle' ? 'Idle' : imuState === 'running' ? 'Läuft…' : '✓ Fertig' }}
          </span>
        </div>
      </div>

      <!-- Log-Stream -->
      <div class="st-subsec">
        <div class="st-loghead">
          <span class="st-sublbl" style="margin-bottom: 0">Log-Stream</span>
          <button class="st-btn st-btn-ghost st-btn-sm" @click="clearLogs">Clear</button>
        </div>
        <div class="st-logwin">
          <div
            v-for="(line, i) in logs"
            :key="i"
            class="st-logline"
          >{{ line }}</div>
          <div v-if="logs.length === 0" class="st-lognone">
            Keine Log-Einträge — WebSocket-Nachrichten mit type:"log" erscheinen hier
          </div>
        </div>
      </div>
    </div>

    <!-- ── Toast ── -->
    <Transition name="toast">
      <div v-if="toast" :class="['st-toast', toast.type === 'success' ? 'st-toast-ok' : 'st-toast-err']">
        {{ toast.text }}
      </div>
    </Transition>

  </div>
</template>

<style scoped>
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

/* ── Page wrapper ── */
.st-page {
  padding: 20px 24px 40px;
  max-width: 760px;
  display: flex;
  flex-direction: column;
  gap: 16px;
  font-family: sans-serif;
  color: #e2e8f0 !important;
  position: relative;
}

/* ── Cards ── */
.st-card {
  background: #0a1020 !important;
  border: 1px solid #1e3a5f;
  border-radius: 10px;
  overflow: hidden;
}

.st-head {
  font-size: 10px;
  font-weight: 500;
  color: #475569 !important;
  text-transform: uppercase;
  letter-spacing: 1px;
  padding: 10px 14px 8px;
  border-bottom: 1px solid #0f1829;
  display: flex;
  align-items: center;
  gap: 8px;
}

/* ── Info rows (Sektion 1) ── */
.st-rows {
  display: flex;
  flex-direction: column;
}
.st-row {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 8px 14px;
  border-bottom: 1px solid #0d1525;
  font-size: 12px;
}
.st-row:last-child { border-bottom: none; }

.st-key {
  width: 140px;
  flex-shrink: 0;
  color: #64748b !important;
}
.st-val {
  color: #e2e8f0 !important;
  display: flex;
  align-items: center;
  gap: 6px;
  flex-wrap: wrap;
}
.st-mono   { font-family: monospace; }
.st-muted  { color: #64748b !important; }
.st-green  { color: #4ade80 !important; }
.st-red    { color: #f87171 !important; }

.st-dot {
  width: 6px; height: 6px;
  border-radius: 50%;
  flex-shrink: 0;
}
.st-dot-green { background: #4ade80; }
.st-dot-red   { background: #f87171; }

.st-link {
  color: #60a5fa !important;
  text-decoration: none;
  font-size: 11px;
  cursor: pointer;
}
.st-link:hover { text-decoration: underline; }
.st-sep { color: #475569 !important; }

/* ── Config fields (Sektion 2) ── */
.st-fields {
  display: flex;
  flex-direction: column;
  gap: 0;
}
.st-field {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 9px 14px;
  border-bottom: 1px solid #0d1525;
}
.st-field:last-child { border-bottom: none; }

.st-flbl {
  flex: 1;
  font-size: 12px;
  color: #94a3b8 !important;
  cursor: default;
}
.st-fkey {
  font-size: 10px;
  font-family: monospace;
  color: #334155 !important;
  width: 220px;
  flex-shrink: 0;
  text-align: right;
}
.st-finput-wrap {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-shrink: 0;
}
.st-finput {
  width: 90px;
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  color: #e2e8f0 !important;
  font-size: 12px;
  font-family: monospace;
  border-radius: 5px;
  padding: 5px 8px;
  outline: none;
  text-align: right;
}
.st-finput:focus { border-color: #2563eb; }
.st-finput-changed {
  border-color: #d97706 !important;
  color: #fbbf24 !important;
}
.st-unit {
  font-size: 11px;
  color: #475569 !important;
  width: 28px;
  flex-shrink: 0;
}

.st-cfooter {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 14px;
  border-top: 1px solid #0f1829;
  background: #070d18 !important;
}
.st-hint {
  font-size: 10px;
  color: #334155 !important;
  font-family: monospace;
}
.st-btnrow {
  display: flex;
  gap: 6px;
}

/* ── Buttons ── */
.st-btn {
  padding: 6px 14px;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
  font-family: sans-serif;
}
.st-btn:disabled { opacity: 0.35; cursor: not-allowed; }

.st-btn-ghost {
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  color: #64748b !important;
}
.st-btn-save {
  background: #0c1a3a !important;
  border: 1px solid #2563eb;
  color: #93c5fd !important;
}
.st-btn-save:not(:disabled):hover {
  background: #112455 !important;
}
.st-btn-motor {
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  color: #60a5fa !important;
  flex: 1;
}
.st-btn-motor:not(:disabled):hover {
  background: #1a2a45 !important;
}
.st-btn-imu {
  background: #1c1200 !important;
  border: 1px solid #d97706;
  color: #fbbf24 !important;
}
.st-btn-sm {
  padding: 3px 10px;
  font-size: 11px;
}

/* ── Diagnose sub-sections ── */
.st-subsec {
  padding: 12px 14px;
  border-bottom: 1px solid #0d1525;
}
.st-subsec:last-child { border-bottom: none; }

.st-sublbl {
  font-size: 10px;
  font-weight: 500;
  color: #475569 !important;
  text-transform: uppercase;
  letter-spacing: 1px;
  margin-bottom: 10px;
}
.st-warn {
  font-size: 11px;
  color: #fbbf24 !important;
  margin-bottom: 10px;
  display: flex;
  align-items: center;
  gap: 5px;
}
.st-motorrow {
  display: flex;
  gap: 6px;
}

.st-imurow {
  display: flex;
  align-items: center;
  gap: 12px;
}
.st-imustatus {
  font-size: 12px;
  font-family: monospace;
}
.st-imu-idle    { color: #475569 !important; }
.st-imu-running { color: #fbbf24 !important; }
.st-imu-done    { color: #4ade80 !important; }

.st-loghead {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 8px;
}
.st-logwin {
  background: #070d18 !important;
  border: 1px solid #1e3a5f;
  border-radius: 6px;
  padding: 8px 10px;
  max-height: 200px;
  overflow-y: auto;
  font-family: monospace;
  font-size: 11px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.st-logline {
  color: #64748b !important;
  white-space: pre-wrap;
  word-break: break-all;
  line-height: 1.5;
}
.st-lognone {
  color: #334155 !important;
  font-style: italic;
  text-align: center;
  padding: 8px 0;
}

/* ── Badges ── */
.st-badge {
  padding: 2px 7px;
  border-radius: 10px;
  font-size: 10px;
  font-weight: 500;
  text-transform: none;
  letter-spacing: 0;
}
.st-badge-muted { background: #0f1829; color: #475569 !important; border: 1px solid #1e3a5f; }
.st-badge-amber { background: #1c1200; color: #fbbf24 !important; border: 1px solid #d97706; }

/* ── Toast ── */
.st-toast {
  position: fixed;
  bottom: 24px;
  right: 24px;
  padding: 10px 16px;
  border-radius: 8px;
  font-size: 12px;
  font-weight: 500;
  font-family: sans-serif;
  z-index: 999;
  pointer-events: none;
}
.st-toast-ok  { background: #052e16; border: 1px solid #166534; color: #4ade80 !important; }
.st-toast-err { background: #1a0505; border: 1px solid #450a0a; color: #f87171 !important; }

.toast-enter-active, .toast-leave-active { transition: opacity 0.25s, transform 0.25s; }
.toast-enter-from, .toast-leave-to { opacity: 0; transform: translateY(8px); }
</style>
