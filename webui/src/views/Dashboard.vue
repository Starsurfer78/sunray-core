<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'
import RobotMap from '../components/RobotMap.vue'

const { telemetry, sendCmd, connected } = useTelemetry()
const mapRef = ref<InstanceType<typeof RobotMap> | null>(null)

// ── Zone selection (C.12) ─────────────────────────────────────────────────────

interface Zone { id: string; settings?: { name?: string } }
const zones          = ref<Zone[]>([])
const selectedZones  = ref<Set<string>>(new Set())
const showZonePicker = ref(false)

async function loadZones() {
  try {
    const r = await fetch('/api/map')
    const m = await r.json()
    zones.value = (m.zones ?? []) as Zone[]
  } catch { /* no map yet */ }
}
onMounted(loadZones)

function toggleZone(id: string) {
  if (selectedZones.value.has(id)) selectedZones.value.delete(id)
  else selectedZones.value.add(id)
}

function startMowWithZones() {
  showZonePicker.value = false
  if (selectedZones.value.size === 0 || selectedZones.value.size === zones.value.length) {
    sendCmd('start')  // all zones = normal start
  } else {
    sendCmd('startZones', { zones: [...selectedZones.value] })
  }
}

const mapBadge = computed(() =>
  telemetry.value.gps_lat !== 0
    ? `${telemetry.value.gps_lat.toFixed(4)}°N  ${telemetry.value.gps_lon.toFixed(4)}°E`
    : 'Kein GPS'
)

// ── Op-Status ─────────────────────────────────────────────────────────────────

const OP_LABEL: Record<string, string> = {
  Idle:          'Bereit',
  Mow:           'Mähen',
  Dock:          'Andocken',
  Charge:        'Laden',
  EscapeReverse: 'Ausweichen',
  GpsWait:       'GPS-Warten',
  Error:         'Fehler',
}
const OP_COLOR: Record<string, string> = {
  Idle:          '#4b6a8a',
  Mow:           '#22c55e',
  Dock:          '#f59e0b',
  Charge:        '#3b82f6',
  EscapeReverse: '#f97316',
  GpsWait:       '#a855f7',
  Error:         '#ef4444',
}

const opLabel = computed(() => OP_LABEL[telemetry.value.op] ?? telemetry.value.op)
const opColor = computed(() => OP_COLOR[telemetry.value.op] ?? '#4b6a8a')

const isMowing  = computed(() => telemetry.value.op === 'Mow')
const isIdle    = computed(() => telemetry.value.op === 'Idle')
const isCharging = computed(() => telemetry.value.op === 'Charge')

// ── Battery ───────────────────────────────────────────────────────────────────

const BATT_MIN = 20.0
const BATT_MAX = 29.4
const battPct = computed(() => {
  const v = telemetry.value.battery_v
  if (v < 1) return 0
  return Math.max(0, Math.min(100, Math.round((v - BATT_MIN) / (BATT_MAX - BATT_MIN) * 100)))
})
const battColor = computed(() =>
  battPct.value > 50 ? '#22c55e' : battPct.value > 20 ? '#f59e0b' : '#ef4444'
)

// ── Commands ──────────────────────────────────────────────────────────────────

let stopConfirmPending = false
let stopConfirmTimer: ReturnType<typeof setTimeout> | null = null

function onStop() {
  if (stopConfirmPending) {
    sendCmd('stop')
    stopConfirmPending = false
    if (stopConfirmTimer) clearTimeout(stopConfirmTimer)
    stopConfirmTimer = null
  } else {
    stopConfirmPending = true
    stopConfirmTimer = setTimeout(() => { stopConfirmPending = false }, 3000)
  }
}
</script>

<template>
  <div class="sr-dash">
    <RobotMap
      ref="mapRef"
      :x="telemetry.x"
      :y="telemetry.y"
      :heading="telemetry.heading"
      :op="telemetry.op"
    />

    <!-- GPS badge -->
    <div class="sr-mbadge">{{ mapBadge }}</div>

    <!-- Zoom controls -->
    <div class="sr-mctrl">
      <button class="sr-mcb" @click="mapRef?.zoomIn()">+</button>
      <button class="sr-mcb" @click="mapRef?.zoomOut()">−</button>
      <button class="sr-mcb" title="Roboter zentrieren" @click="mapRef?.centerRobot()">⊙</button>
    </div>

    <!-- Zone picker modal (C.12) -->
    <Transition name="modal">
      <div v-if="showZonePicker" class="sr-overlay" @click.self="showZonePicker = false">
        <div class="sr-picker">
          <div class="sr-picker-hdr">Zonen auswählen</div>
          <div class="sr-picker-hint">Leer = alle Zonen mähen</div>
          <div class="sr-picker-zones">
            <button
              v-for="z in zones"
              :key="z.id"
              class="sr-zone-btn"
              :class="{ selected: selectedZones.has(z.id) }"
              @click="toggleZone(z.id)">
              {{ z.settings?.name ?? z.id }}
            </button>
          </div>
          <div class="sr-picker-actions">
            <button class="sr-btn sr-btn-mow" @click="startMowWithZones">
              Mähen starten
            </button>
            <button class="sr-btn-cancel" @click="showZonePicker = false">Abbrechen</button>
          </div>
        </div>
      </div>
    </Transition>

    <!-- Control panel (bottom right) -->
    <div class="sr-panel">

      <!-- Op status + connection -->
      <div class="sr-status-row">
        <span class="sr-dot" :style="{ background: connected ? '#22c55e' : '#ef4444' }" />
        <span class="sr-op-badge" :style="{ color: opColor, borderColor: opColor + '44' }">
          {{ opLabel }}
        </span>
        <span v-if="telemetry.motor_err" class="sr-err-tag">Motor-Fehler</span>
      </div>

      <!-- Battery bar -->
      <div class="sr-batt-row" :title="`${telemetry.battery_v.toFixed(1)} V`">
        <span class="sr-batt-lbl">Akku</span>
        <div class="sr-batt-track">
          <div class="sr-batt-fill"
            :style="{ width: battPct + '%', background: battColor }" />
        </div>
        <span class="sr-batt-pct" :style="{ color: battColor }">{{ battPct }}%</span>
        <span v-if="isCharging" class="sr-charging-icon">⚡</span>
      </div>

      <!-- Main control buttons -->
      <div class="sr-btns">
        <button
          class="sr-btn sr-btn-mow"
          :disabled="!connected || isMowing"
          @click="zones.length > 1 ? showZonePicker = true : sendCmd('start')"
          title="Mähauftrag starten">
          Mähen
        </button>
        <button
          class="sr-btn sr-btn-dock"
          :disabled="!connected || isIdle || isCharging"
          @click="sendCmd('dock')"
          title="Zur Ladestation fahren">
          Andocken
        </button>
        <button
          class="sr-btn sr-btn-stop"
          :class="{ 'sr-btn-stop-confirm': stopConfirmPending }"
          :disabled="!connected || isIdle || isCharging"
          @click="onStop"
          :title="stopConfirmPending ? 'Nochmal klicken zum Bestätigen' : 'Nothalt'">
          {{ stopConfirmPending ? 'Bestätigen?' : 'STOP' }}
        </button>
      </div>

    </div>
  </div>
</template>

<style scoped>
.sr-dash {
  flex: 1;
  position: relative;
  background: #070d18 !important;
  overflow: hidden;
  height: 100%;
}

/* GPS badge */
.sr-mbadge {
  position: absolute; top: 10px; left: 10px;
  background: rgba(15,24,41,0.88); border: 1px solid #1e3a5f;
  border-radius: 7px; padding: 6px 10px; font-size: 11px;
  color: #60a5fa !important; pointer-events: none; font-family: sans-serif;
}

/* Zoom controls */
.sr-mctrl {
  position: absolute; bottom: 12px; left: 12px; display: flex; gap: 5px;
}
.sr-mcb {
  background: #0f1829 !important; border: 1px solid #1e3a5f;
  color: #60a5fa !important; border-radius: 6px; padding: 5px 9px;
  font-size: 13px; cursor: pointer; font-family: sans-serif;
}

/* Control panel */
.sr-panel {
  position: absolute; bottom: 12px; right: 12px;
  background: rgba(10, 15, 26, 0.92); border: 1px solid #1e3a5f;
  border-radius: 12px; padding: 12px 14px;
  display: flex; flex-direction: column; gap: 10px;
  min-width: 200px; font-family: sans-serif; backdrop-filter: blur(4px);
}

/* Op status row */
.sr-status-row {
  display: flex; align-items: center; gap: 7px;
}
.sr-dot {
  width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0;
}
.sr-op-badge {
  font-size: 12px; font-weight: 600;
  border: 1px solid; border-radius: 5px; padding: 2px 8px;
}
.sr-err-tag {
  font-size: 10px; color: #ef4444 !important;
  background: #2a0a0a; border: 1px solid #7f1d1d;
  border-radius: 4px; padding: 1px 5px;
}

/* Battery */
.sr-batt-row {
  display: flex; align-items: center; gap: 6px;
}
.sr-batt-lbl {
  font-size: 10px; color: #4b6a8a !important; width: 28px;
}
.sr-batt-track {
  flex: 1; height: 6px; background: #1e2d3d; border-radius: 3px; overflow: hidden;
}
.sr-batt-fill {
  height: 100%; border-radius: 3px; transition: width 0.5s;
}
.sr-batt-pct {
  font-size: 11px; font-weight: 600; width: 34px; text-align: right;
}
.sr-charging-icon {
  font-size: 12px;
}

/* Buttons */
.sr-btns {
  display: flex; gap: 6px;
}
.sr-btn {
  flex: 1; padding: 8px 6px; font-size: 12px; font-weight: 600;
  border-radius: 7px; border: 1px solid; cursor: pointer;
  transition: opacity 0.15s, transform 0.1s;
  font-family: sans-serif;
}
.sr-btn:disabled {
  opacity: 0.35; cursor: not-allowed;
}
.sr-btn:not(:disabled):active { transform: scale(0.96); }

.sr-btn-mow {
  background: #14532d !important; border-color: #22c55e;
  color: #86efac !important;
}
.sr-btn-mow:not(:disabled):hover { background: #166534 !important; }

.sr-btn-dock {
  background: #451a03 !important; border-color: #f59e0b;
  color: #fcd34d !important;
}
.sr-btn-dock:not(:disabled):hover { background: #78350f !important; }

.sr-btn-stop {
  background: #450a0a !important; border-color: #ef4444;
  color: #fca5a5 !important;
}
.sr-btn-stop:not(:disabled):hover { background: #7f1d1d !important; }
.sr-btn-stop-confirm {
  background: #ef4444 !important; border-color: #fca5a5;
  color: #fff !important; animation: pulse 0.5s infinite alternate;
}
@keyframes pulse { from { opacity: 0.8; } to { opacity: 1; } }

/* Zone picker */
.sr-overlay {
  position: fixed; inset: 0; background: rgba(0,0,0,0.7);
  display: flex; align-items: center; justify-content: center; z-index: 50;
}
.sr-picker {
  background: #0f1829 !important; border: 1px solid #1e3a5f; border-radius: 14px;
  padding: 20px; width: min(340px, 90vw); display: flex; flex-direction: column; gap: 12px;
  font-family: sans-serif;
}
.sr-picker-hdr  { font-size: 14px; font-weight: 600; color: #e2e8f0 !important; }
.sr-picker-hint { font-size: 11px; color: #4b6a8a !important; }
.sr-picker-zones { display: flex; flex-wrap: wrap; gap: 8px; }
.sr-zone-btn {
  padding: 7px 14px; font-size: 12px; border-radius: 7px;
  border: 1px solid #1e3a5f; background: #070d18 !important;
  color: #64748b !important; cursor: pointer; font-family: sans-serif;
  transition: border-color 0.15s, color 0.15s, background 0.15s;
}
.sr-zone-btn.selected {
  border-color: #22c55e; background: #14532d !important; color: #86efac !important;
}
.sr-picker-actions { display: flex; align-items: center; gap: 10px; }
.sr-btn-cancel {
  font-size: 12px; color: #4b6a8a !important; background: none; border: none; cursor: pointer;
  font-family: sans-serif;
}
.modal-enter-active, .modal-leave-active { transition: opacity 0.2s; }
.modal-enter-from, .modal-leave-to { opacity: 0; }
</style>
