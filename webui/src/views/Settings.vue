<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useTelemetry } from '../composables/useTelemetry'

const router = useRouter()
const { telemetry, connected } = useTelemetry()

// ── Uptime ────────────────────────────────────────────────────────────────────
const uptime = computed(() => {
  const s = telemetry.value.uptime_s
  const h = Math.floor(s / 3600)
  const m = Math.floor((s % 3600) / 60)
  return `${h}h ${String(m).padStart(2, '0')}m`
})

// ── System info (Sektion 1) ───────────────────────────────────────────────────
const piVersion  = ref('—')
const mcuVersion = ref('—')
const mapPath    = ref('/etc/sunray/map.json')

async function fetchVersion() {
  try {
    const res = await fetch('/api/version')
    if (res.ok) {
      const j = await res.json()
      piVersion.value  = j.pi  || '—'
      mcuVersion.value = j.mcu || '—'
    }
  } catch { /* backend may not have /api/version */ }
}

// ── Unified config store ───────────────────────────────────────────────────────

const cfg     = ref<Record<string, any>>({})
const cfgOrig = ref<Record<string, any>>({})
const cfgLoading = ref(false)

// Fallback defaults (used when a key is absent in the API response)
const DEFAULTS: Record<string, any> = {
  // Fahrt
  motor_set_speed_ms:         0.3,
  dock_linear_speed_ms:       0.1,
  target_reached_tolerance_m: 0.1,
  rain_delay_min:             60,
  strip_width_default:        0.25,
  speed_default:              0.3,
  // Akku
  battery_low_v:              25.5,
  battery_critical_v:         18.9,
  battery_full_v:             30.0,
  battery_full_current_a:     -0.1,
  battery_full_slope:         0.002,
  // Temperatur
  overheat_temp_c:            85,
  too_cold_temp_c:            5,
  // Motor PID
  motor_pid_kp:               0.5,
  motor_pid_ki:               0.01,
  motor_pid_kd:               0.01,
  // Motor Strom
  motor_fault_current_a:      3.0,
  motor_overload_current_a:   1.2,
  mow_fault_current_a:        8.0,
  mow_overload_current_a:     2.0,
  // GPS
  gps_no_motion_threshold_m:  0.05,
  // MQTT
  mqtt_enabled:               false,
  mqtt_host:                  'localhost',
  mqtt_port:                  1883,
  mqtt_keepalive_s:           60,
  mqtt_topic_prefix:          'sunray',
  mqtt_user:                  '',
  mqtt_pass:                  '',
  // NTRIP
  ntrip_enabled:              false,
  ntrip_host:                 '',
  ntrip_port:                 2101,
  ntrip_mount:                '',
  ntrip_user:                 '',
  ntrip_pass:                 '',
}

async function loadConfig() {
  cfgLoading.value = true
  try {
    const res = await fetch('/api/config')
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    const json = await res.json()
    const merged: Record<string, any> = { ...DEFAULTS }
    for (const [k, v] of Object.entries(json)) merged[k] = v
    cfg.value     = { ...merged }
    cfgOrig.value = { ...merged }
    if (typeof json['map_path'] === 'string') mapPath.value = json['map_path']
  } catch (e) {
    showToast(`Config laden: ${e}`, 'error')
  } finally {
    cfgLoading.value = false
  }
}

function isKeyDirty(k: string): boolean {
  return JSON.stringify(cfg.value[k]) !== JSON.stringify(cfgOrig.value[k])
}

async function saveKeys(keys: string[], label: string) {
  const patch: Record<string, any> = {}
  for (const k of keys) {
    if (isKeyDirty(k)) patch[k] = cfg.value[k]
  }
  if (Object.keys(patch).length === 0) return
  try {
    const res = await fetch('/api/config', {
      method:  'PUT',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(patch),
    })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    for (const k of keys) cfgOrig.value[k] = cfg.value[k]
    showToast(`${label} gespeichert`, 'success')
  } catch (e) {
    showToast(`Speichern fehlgeschlagen: ${e}`, 'error')
  }
}

// ── Sektion 2: Roboter-Konfiguration ─────────────────────────────────────────

interface NumField {
  key: string; label: string; unit: string; step: number; min: number; max: number
}
interface FieldGroup { label: string; fields: NumField[] }

const FIELD_GROUPS: FieldGroup[] = [
  {
    label: 'Fahrt',
    fields: [
      { key: 'motor_set_speed_ms',         label: 'Mähgeschwindigkeit',   unit: 'm/s', step: 0.01, min: 0.05, max: 0.8  },
      { key: 'dock_linear_speed_ms',       label: 'Dock-Geschwindigkeit', unit: 'm/s', step: 0.01, min: 0.01, max: 0.3  },
      { key: 'target_reached_tolerance_m', label: 'Ziel-Toleranz',        unit: 'm',   step: 0.01, min: 0.05, max: 0.5  },
      { key: 'rain_delay_min',             label: 'Regen-Verzögerung',    unit: 'min', step: 5,    min: 0,    max: 240  },
    ],
  },
  {
    label: 'Akku',
    fields: [
      { key: 'battery_low_v',          label: 'Warnschwelle (Dock)',    unit: 'V', step: 0.1,  min: 18,  max: 30   },
      { key: 'battery_critical_v',     label: 'Kritisch (Sofortstopp)', unit: 'V', step: 0.1,  min: 16,  max: 28   },
      { key: 'battery_full_v',         label: 'Voll (Mähen fortsetzen)',unit: 'V', step: 0.1,  min: 24,  max: 35   },
      { key: 'battery_full_current_a', label: 'Voll bei Strom ≤',       unit: 'A', step: 0.01, min: -2,  max: 0    },
    ],
  },
  {
    label: 'Temperatur',
    fields: [
      { key: 'overheat_temp_c', label: 'Überhitzung → Dock', unit: '°C', step: 1, min: 50, max: 100 },
      { key: 'too_cold_temp_c', label: 'Zu kalt → Dock',     unit: '°C', step: 1, min: -10, max: 20  },
    ],
  },
  {
    label: 'Motor PID',
    fields: [
      { key: 'motor_pid_kp', label: 'KP', unit: '', step: 0.01,  min: 0, max: 5 },
      { key: 'motor_pid_ki', label: 'KI', unit: '', step: 0.001, min: 0, max: 1 },
      { key: 'motor_pid_kd', label: 'KD', unit: '', step: 0.001, min: 0, max: 1 },
    ],
  },
  {
    label: 'Motorstrom',
    fields: [
      { key: 'motor_fault_current_a',    label: 'Fahrmotoren Fehler (aktuell ungenutzt)',   unit: 'A', step: 0.1, min: 0, max: 10 },
      { key: 'motor_overload_current_a', label: 'Fahrmotoren Überlast (aktuell ungenutzt)', unit: 'A', step: 0.1, min: 0, max: 5  },
      { key: 'mow_fault_current_a',      label: 'Mähmotor Fehler (aktuell ungenutzt)',      unit: 'A', step: 0.1, min: 0, max: 15 },
      { key: 'mow_overload_current_a',   label: 'Mähmotor Überlast (aktuell ungenutzt)',    unit: 'A', step: 0.1, min: 0, max: 8  },
    ],
  },
  {
    label: 'GPS',
    fields: [
      { key: 'gps_no_motion_threshold_m', label: 'No-Motion Schwelle', unit: 'm', step: 0.01, min: 0.01, max: 0.5 },
    ],
  },
]

const ROBOT_KEYS = FIELD_GROUPS.flatMap(g => g.fields.map(f => f.key))
const robotDirty = computed(() => ROBOT_KEYS.some(isKeyDirty))
const robotSaving = ref(false)

async function saveRobot() {
  robotSaving.value = true
  await saveKeys(ROBOT_KEYS, 'Roboter-Konfiguration')
  robotSaving.value = false
}

// ── Sektion 3: MQTT ───────────────────────────────────────────────────────────

interface MixedField {
  key: string; label: string; type: 'toggle' | 'text' | 'password' | 'number'
  unit?: string; step?: number; min?: number; max?: number
}

const MQTT_FIELDS: MixedField[] = [
  { key: 'mqtt_enabled',      label: 'Aktiviert',    type: 'toggle'                                     },
  { key: 'mqtt_host',         label: 'Broker Host',  type: 'text'                                       },
  { key: 'mqtt_port',         label: 'Port',         type: 'number', step: 1,  min: 1, max: 65535       },
  { key: 'mqtt_keepalive_s',  label: 'Keepalive',    type: 'number', unit: 's', step: 1, min: 5, max: 300 },
  { key: 'mqtt_topic_prefix', label: 'Topic-Prefix', type: 'text'                                       },
  { key: 'mqtt_user',         label: 'Benutzer',     type: 'text'                                       },
  { key: 'mqtt_pass',         label: 'Passwort',     type: 'password'                                   },
]

const MQTT_KEYS   = MQTT_FIELDS.map(f => f.key)
const mqttDirty   = computed(() => MQTT_KEYS.some(isKeyDirty))
const mqttSaving  = ref(false)

async function saveMqtt() {
  mqttSaving.value = true
  await saveKeys(MQTT_KEYS, 'MQTT')
  mqttSaving.value = false
}

// ── Sektion 4: NTRIP ─────────────────────────────────────────────────────────

const NTRIP_FIELDS: MixedField[] = [
  { key: 'ntrip_enabled', label: 'Aktiviert',  type: 'toggle'                                       },
  { key: 'ntrip_host',    label: 'Host',        type: 'text'                                         },
  { key: 'ntrip_port',    label: 'Port',        type: 'number', step: 1,  min: 1, max: 65535         },
  { key: 'ntrip_mount',   label: 'Mountpoint',  type: 'text'                                         },
  { key: 'ntrip_user',    label: 'Benutzer',    type: 'text'                                         },
  { key: 'ntrip_pass',    label: 'Passwort',    type: 'password'                                     },
]

const NTRIP_KEYS  = NTRIP_FIELDS.map(f => f.key)
const ntripDirty  = computed(() => NTRIP_KEYS.some(isKeyDirty))
const ntripSaving = ref(false)

async function saveNtrip() {
  ntripSaving.value = true
  await saveKeys(NTRIP_KEYS, 'NTRIP')
  ntripSaving.value = false
}

// ── Toast ─────────────────────────────────────────────────────────────────────
const toast      = ref<{ text: string; type: 'success' | 'error' } | null>(null)
let   toastTimer: ReturnType<typeof setTimeout> | null = null

function showToast(text: string, type: 'success' | 'error') {
  if (toastTimer) clearTimeout(toastTimer)
  toast.value = { text, type }
  toastTimer  = setTimeout(() => { toast.value = null }, 2500)
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
          <span class="st-key">Pi Software</span>
          <span class="st-val st-mono">v{{ piVersion }}</span>
        </div>
        <div class="st-row">
          <span class="st-key">Alfred Firmware</span>
          <span class="st-val st-mono">{{ mcuVersion }}</span>
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
            <a class="st-link" @click.prevent="router.push('/map')" href="#">Karte öffnen →</a>
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
        <span v-if="robotDirty" class="st-badge st-badge-amber">Ungespeichert</span>
      </div>

      <template v-for="group in FIELD_GROUPS" :key="group.label">
        <div class="st-grplbl">{{ group.label }}</div>
        <div v-for="f in group.fields" :key="f.key" class="st-field">
          <label class="st-flbl">{{ f.label }}</label>
          <div class="st-finput-wrap">
            <input
              v-model.number="cfg[f.key]"
              type="number"
              class="st-finput"
              :step="f.step" :min="f.min" :max="f.max"
              :class="{ 'st-finput-changed': isKeyDirty(f.key) }"
            />
            <span class="st-unit">{{ f.unit }}</span>
          </div>
          <span class="st-fkey">{{ f.key }}</span>
        </div>
      </template>

      <div class="st-cfooter">
        <span class="st-hint">/etc/sunray/config.json</span>
        <div class="st-btnrow">
          <button class="st-btn st-btn-ghost" @click="loadConfig" :disabled="cfgLoading">↺ Neu laden</button>
          <button class="st-btn st-btn-save" :disabled="!robotDirty || robotSaving" @click="saveRobot">
            {{ robotSaving ? 'Speichert…' : '✓ Speichern' }}
          </button>
        </div>
      </div>
    </div>

    <!-- ════════════════════════════════════════════
         SEKTION 3 — MQTT
         ════════════════════════════════════════════ -->
    <div class="st-card">
      <div class="st-head">
        MQTT
        <span v-if="mqttDirty" class="st-badge st-badge-amber">Ungespeichert</span>
        <span class="st-badge" :class="cfg['mqtt_enabled'] ? 'st-badge-green' : 'st-badge-muted'">
          {{ cfg['mqtt_enabled'] ? 'Aktiv' : 'Deaktiviert' }}
        </span>
      </div>

      <div v-for="f in MQTT_FIELDS" :key="f.key" class="st-field">
        <label class="st-flbl">{{ f.label }}</label>
        <div class="st-finput-wrap">
          <!-- Toggle -->
          <template v-if="f.type === 'toggle'">
            <label class="st-tgl" :class="{ 'st-tgl-changed': isKeyDirty(f.key) }">
              <input type="checkbox" class="st-tgl-input"
                :checked="!!cfg[f.key]"
                @change="cfg[f.key] = ($event.target as HTMLInputElement).checked"
              />
              <span class="st-tgl-track"><span class="st-tgl-thumb"></span></span>
              <span class="st-tgl-val">{{ cfg[f.key] ? 'Ein' : 'Aus' }}</span>
            </label>
          </template>
          <!-- Number -->
          <template v-else-if="f.type === 'number'">
            <input
              v-model.number="cfg[f.key]"
              type="number" class="st-finput"
              :step="f.step" :min="f.min" :max="f.max"
              :class="{ 'st-finput-changed': isKeyDirty(f.key) }"
            />
            <span class="st-unit">{{ f.unit ?? '' }}</span>
          </template>
          <!-- Text / Password -->
          <template v-else>
            <input
              v-model="cfg[f.key]"
              :type="f.type"
              class="st-finput st-finput-str"
              :class="{ 'st-finput-changed': isKeyDirty(f.key) }"
              autocomplete="off"
            />
          </template>
        </div>
        <span class="st-fkey">{{ f.key }}</span>
      </div>

      <div class="st-cfooter">
        <span class="st-hint">Broker-Reconnect: automatisch (1–30 s)</span>
        <div class="st-btnrow">
          <button class="st-btn st-btn-ghost" @click="loadConfig" :disabled="cfgLoading">↺ Neu laden</button>
          <button class="st-btn st-btn-save" :disabled="!mqttDirty || mqttSaving" @click="saveMqtt">
            {{ mqttSaving ? 'Speichert…' : '✓ Speichern' }}
          </button>
        </div>
      </div>
    </div>

    <!-- ════════════════════════════════════════════
         SEKTION 4 — NTRIP
         ════════════════════════════════════════════ -->
    <div class="st-card">
      <div class="st-head">
        NTRIP (RTK-Korrekturen)
        <span v-if="ntripDirty" class="st-badge st-badge-amber">Ungespeichert</span>
        <span class="st-badge" :class="cfg['ntrip_enabled'] ? 'st-badge-green' : 'st-badge-muted'">
          {{ cfg['ntrip_enabled'] ? 'Aktiv' : 'Deaktiviert' }}
        </span>
      </div>

      <div v-for="f in NTRIP_FIELDS" :key="f.key" class="st-field">
        <label class="st-flbl">{{ f.label }}</label>
        <div class="st-finput-wrap">
          <template v-if="f.type === 'toggle'">
            <label class="st-tgl" :class="{ 'st-tgl-changed': isKeyDirty(f.key) }">
              <input type="checkbox" class="st-tgl-input"
                :checked="!!cfg[f.key]"
                @change="cfg[f.key] = ($event.target as HTMLInputElement).checked"
              />
              <span class="st-tgl-track"><span class="st-tgl-thumb"></span></span>
              <span class="st-tgl-val">{{ cfg[f.key] ? 'Ein' : 'Aus' }}</span>
            </label>
          </template>
          <template v-else-if="f.type === 'number'">
            <input
              v-model.number="cfg[f.key]"
              type="number" class="st-finput"
              :step="f.step" :min="f.min" :max="f.max"
              :class="{ 'st-finput-changed': isKeyDirty(f.key) }"
            />
            <span class="st-unit">{{ f.unit ?? '' }}</span>
          </template>
          <template v-else>
            <input
              v-model="cfg[f.key]"
              :type="f.type"
              class="st-finput st-finput-str"
              :class="{ 'st-finput-changed': isKeyDirty(f.key) }"
              autocomplete="off"
            />
          </template>
        </div>
        <span class="st-fkey">{{ f.key }}</span>
      </div>

      <div class="st-cfooter">
        <span class="st-hint">RTCM-Korrekturen via TCP → ZED-F9P USB</span>
        <div class="st-btnrow">
          <button class="st-btn st-btn-ghost" @click="loadConfig" :disabled="cfgLoading">↺ Neu laden</button>
          <button class="st-btn st-btn-save" :disabled="!ntripDirty || ntripSaving" @click="saveNtrip">
            {{ ntripSaving ? 'Speichert…' : '✓ Speichern' }}
          </button>
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
.st-rows { display: flex; flex-direction: column; }
.st-row {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 8px 14px;
  border-bottom: 1px solid #0d1525;
  font-size: 12px;
}
.st-row:last-child { border-bottom: none; }
.st-key { width: 140px; flex-shrink: 0; color: #64748b !important; }
.st-val { color: #e2e8f0 !important; display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }
.st-mono   { font-family: monospace; }
.st-muted  { color: #64748b !important; }
.st-green  { color: #4ade80 !important; }
.st-red    { color: #f87171 !important; }
.st-dot { width: 6px; height: 6px; border-radius: 50%; flex-shrink: 0; }
.st-dot-green { background: #4ade80; }
.st-dot-red   { background: #f87171; }
.st-link { color: #60a5fa !important; text-decoration: none; font-size: 11px; cursor: pointer; }
.st-link:hover { text-decoration: underline; }

/* ── Group label ── */
.st-grplbl {
  font-size: 10px;
  font-weight: 500;
  color: #334155 !important;
  text-transform: uppercase;
  letter-spacing: 1px;
  padding: 8px 14px 4px;
  background: #070d18 !important;
  border-bottom: 1px solid #0d1525;
}

/* ── Config fields ── */
.st-field {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 9px 14px;
  border-bottom: 1px solid #0d1525;
}
.st-field:last-of-type { border-bottom: none; }

.st-flbl { flex: 1; font-size: 12px; color: #94a3b8 !important; cursor: default; }
.st-fkey {
  font-size: 10px;
  font-family: monospace;
  color: #334155 !important;
  width: 220px;
  flex-shrink: 0;
  text-align: right;
}
.st-finput-wrap { display: flex; align-items: center; gap: 6px; flex-shrink: 0; }

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
.st-finput-str { width: 160px; text-align: left; }
.st-finput:focus { border-color: #2563eb; }
.st-finput-changed { border-color: #d97706 !important; color: #fbbf24 !important; }

.st-unit { font-size: 11px; color: #475569 !important; width: 28px; flex-shrink: 0; }

/* ── Toggle ── */
.st-tgl { display: flex; align-items: center; gap: 8px; cursor: pointer; user-select: none; }
.st-tgl-input { display: none; }
.st-tgl-track {
  width: 36px; height: 20px;
  background: #1e3a5f;
  border: 1px solid #2d4f7a;
  border-radius: 10px;
  position: relative;
  transition: background 0.2s, border-color 0.2s;
  flex-shrink: 0;
}
.st-tgl-input:checked + .st-tgl-track {
  background: #1d4ed8;
  border-color: #2563eb;
}
.st-tgl-thumb {
  position: absolute;
  top: 2px; left: 2px;
  width: 14px; height: 14px;
  background: #475569;
  border-radius: 50%;
  transition: transform 0.2s, background 0.2s;
}
.st-tgl-input:checked + .st-tgl-track .st-tgl-thumb {
  transform: translateX(16px);
  background: #fff;
}
.st-tgl-val { font-size: 11px; color: #64748b !important; font-family: monospace; width: 24px; }
.st-tgl-changed .st-tgl-track { border-color: #d97706 !important; }

/* ── Footer ── */
.st-cfooter {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 14px;
  border-top: 1px solid #0f1829;
  background: #070d18 !important;
}
.st-hint { font-size: 10px; color: #334155 !important; font-family: monospace; }
.st-btnrow { display: flex; gap: 6px; }

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
.st-btn-ghost { background: #0f1829 !important; border: 1px solid #1e3a5f; color: #64748b !important; }
.st-btn-save  { background: #0c1a3a !important; border: 1px solid #2563eb; color: #93c5fd !important; }
.st-btn-save:not(:disabled):hover { background: #112455 !important; }
.st-btn-sm { padding: 3px 10px; font-size: 11px; }

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
.st-badge-green { background: #052e16; color: #4ade80 !important; border: 1px solid #166534; }

/* ── Toast ── */
.st-toast {
  position: fixed;
  bottom: 24px; right: 24px;
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
