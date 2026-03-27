<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'
import RobotMap from '../components/RobotMap.vue'

const { telemetry, sendCmd, connected } = useTelemetry()
const mapRef = ref<InstanceType<typeof RobotMap> | null>(null)

type Pt = [number, number]
interface CaptureSample {
  x: number
  y: number
  acc: number
  sol: number
}
interface CapturePointMeta {
  fix_duration_ms: number
  sample_variance: number
  mean_accuracy: number
}
interface CaptureMeta {
  perimeter: CapturePointMeta[]
}
interface MapData {
  perimeter: Pt[]
  mow: unknown[]
  dock: Pt[]
  exclusions: Pt[][]
  dockPath?: Pt[]
  zones?: Zone[]
  origin?: { lat: number; lon: number }
  captureMeta?: CaptureMeta
}

// ── Zone selection (C.12) ─────────────────────────────────────────────────────

interface Zone { id: string; settings?: { name?: string } }
const zones          = ref<Zone[]>([])
const selectedZones  = ref<Set<string>>(new Set())
const showZonePicker = ref(false)
const mapData        = ref<MapData>({ perimeter: [], mow: [], dock: [], exclusions: [] })
const captureArmed   = ref(false)
const captureBusy    = ref(false)
const captureStatus  = ref('')
const capturePromptOpen = ref(false)
const capturePromptMode = ref<'decision' | 'result'>('decision')
const capturePromptText = ref('')
const capturePromptResultTone = ref<'success' | 'error'>('success')
const captureProgress = ref(0)
const captureSampleCount = ref(0)
const captureSamplingActive = ref(false)
const captureLastVariance = ref<number | null>(null)
const captureLastMeanAccuracy = ref<number | null>(null)
const captureLastFixDurationMs = ref<number | null>(null)

const CAPTURE_SAMPLE_WINDOW_MS = 3000
const CAPTURE_SAMPLE_INTERVAL_MS = 200
let captureTimer: ReturnType<typeof setInterval> | null = null

function apiHeaders() {
  const token = localStorage.getItem('sunray_api_token') ?? ''
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
  }
  if (token) headers['X-Api-Token'] = token
  return headers
}

async function loadMapData() {
  try {
    const r = await fetch('/api/map', { headers: apiHeaders() })
    if (!r.ok) return
    const m = await r.json()
    mapData.value = {
      perimeter: m.perimeter ?? [],
      mow: m.mow ?? [],
      dock: m.dock ?? [],
      exclusions: m.exclusions ?? [],
      dockPath: m.dockPath ?? [],
      zones: (m.zones ?? []) as Zone[],
      origin: m.origin,
      captureMeta: {
        perimeter: m.captureMeta?.perimeter ?? [],
      },
    }
  } catch { /* no map yet */ }
}

async function loadZones() {
  try {
    const r = await fetch('/api/map', { headers: apiHeaders() })
    const m = await r.json()
    zones.value = (m.zones ?? []) as Zone[]
  } catch { /* no map yet */ }
}
onMounted(async () => {
  await loadMapData()
  await loadZones()
})
onUnmounted(() => {
  if (captureTimer) clearInterval(captureTimer)
  captureTimer = null
})

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
  idle:                'Bereit',
  undocking:           'Ausparken',
  navigating_to_start: 'Zum Startpunkt',
  mowing:              'Mähen',
  docking:             'Andocken',
  charging:            'Laden',
  waiting_for_rain:    'Regenpause',
  obstacle_recovery:   'Ausweichen',
  gps_recovery:        'GPS-Warten',
  fault:               'Fehler',
}
const OP_COLOR: Record<string, string> = {
  idle:                '#4b6a8a',
  undocking:           '#0ea5e9',
  navigating_to_start: '#14b8a6',
  mowing:              '#22c55e',
  docking:             '#f59e0b',
  charging:            '#3b82f6',
  waiting_for_rain:    '#64748b',
  obstacle_recovery:   '#f97316',
  gps_recovery:        '#a855f7',
  fault:               '#ef4444',
}

const opLabel = computed(() => OP_LABEL[telemetry.value.state_phase ?? 'idle'] ?? telemetry.value.op)
const opColor = computed(() => OP_COLOR[telemetry.value.state_phase ?? 'idle'] ?? '#4b6a8a')

const isMowing  = computed(() => telemetry.value.state_phase === 'mowing')
const isIdle    = computed(() => telemetry.value.state_phase === 'idle')
const isCharging = computed(() => telemetry.value.state_phase === 'charging')
const hasRtkFix = computed(() => telemetry.value.gps_sol === 4)
const hasRtkFloat = computed(() => telemetry.value.gps_sol === 5)
const hasRtkSignal = computed(() => hasRtkFix.value || hasRtkFloat.value)
const canStartCaptureSave = computed(() =>
  connected.value && captureArmed.value && !captureBusy.value
)
const rtkQualityLabel = computed(() => {
  if (hasRtkFix.value) return 'RTK-FIX'
  if (hasRtkFloat.value) return 'RTK-FLOAT'
  return telemetry.value.gps_text && telemetry.value.gps_text !== '---'
    ? telemetry.value.gps_text
    : 'Kein RTK'
})
const rtkQualityClass = computed(() => {
  if (hasRtkFix.value) return 'sr-fix-ok'
  if (hasRtkFloat.value) return 'sr-fix-float'
  return 'sr-fix-bad'
})
const rtkAccuracyLabel = computed(() =>
  telemetry.value.gps_acc > 0
    ? `±${telemetry.value.gps_acc.toFixed(3)} m`
    : '—'
)
const captureProgressLabel = computed(() =>
  captureSamplingActive.value
    ? `${captureProgress.value}% · ${captureSampleCount.value} Proben`
    : 'Bereit'
)
const captureStatsLabel = computed(() => {
  const parts: string[] = []
  if (captureLastMeanAccuracy.value !== null) {
    parts.push(`Genauigkeit Ø ${captureLastMeanAccuracy.value.toFixed(3)} m`)
  }
  if (captureLastVariance.value !== null) {
    parts.push(`Varianz ${captureLastVariance.value.toFixed(4)} m²`)
  }
  if (captureLastFixDurationMs.value !== null) {
    parts.push(`Fix-Dauer ${Math.round(captureLastFixDurationMs.value / 1000)} s`)
  }
  return parts.join(' · ')
})
const capturePromptTone = computed(() => {
  if (capturePromptMode.value === 'result') return capturePromptResultTone.value
  if (hasRtkFix.value) return 'success'
  if (hasRtkFloat.value) return 'warning'
  return 'error'
})
const capturePromptTitle = computed(() => {
  if (capturePromptMode.value === 'result') return 'Punktaufnahme'
  if (hasRtkFix.value) return 'RTK-FIX'
  if (hasRtkFloat.value) return 'RTK-FLOAT'
  return 'RTK verloren'
})
const capturePromptMessage = computed(() => {
  if (capturePromptMode.value === 'result') return capturePromptText.value
  if (hasRtkFix.value) {
    return 'RTK-FIX ist vorhanden. Punkt kann jetzt sicher gespeichert werden.'
  }
  if (hasRtkFloat.value) {
    return 'Nur RTK-FLOAT vorhanden. Punkt trotzdem speichern?'
  }
  return 'Kein Speichern moeglich, solange kein RTK-Signal verfuegbar ist.'
})
const capturePromptOkLabel = computed(() =>
  capturePromptMode.value === 'result' ? 'Schliessen' : 'OK'
)
const capturePromptCanConfirm = computed(() =>
  capturePromptMode.value === 'result' || hasRtkSignal.value
)

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

// ── Manual map capture (C.14 start slice) ────────────────────────────────────

async function startNewMapCapture() {
  captureBusy.value = true
  try {
    const nextMap: MapData = {
      perimeter: [],
      mow: [],
      dock: [],
      exclusions: [],
      dockPath: [],
      zones: [],
      captureMeta: { perimeter: [] },
    }
    const res = await fetch('/api/map', {
      method: 'POST',
      headers: apiHeaders(),
      body: JSON.stringify(nextMap),
    })
    const j = await res.json().catch(() => ({ ok: false }))
    if (!res.ok || !j.ok) {
      captureStatus.value = 'Neue Karte konnte nicht angelegt werden.'
      return
    }
    mapData.value = nextMap
    zones.value = []
    selectedZones.value.clear()
    captureArmed.value = true
    capturePromptOpen.value = false
    captureProgress.value = 0
    captureSampleCount.value = 0
    captureSamplingActive.value = false
    captureStatus.value = 'Neue Kartenaufnahme gestartet. Punkt für Punkt entlang der Grenze aufnehmen.'
  } catch {
    captureStatus.value = 'Fehler beim Starten der Kartenaufnahme.'
  } finally {
    captureBusy.value = false
  }
}

function finishCaptureSampling() {
  if (captureTimer) clearInterval(captureTimer)
  captureTimer = null
  captureBusy.value = false
  captureSamplingActive.value = false
}

async function persistCurrentGpsPoint(point: Pt, stats: { variance: number; meanAccuracy: number; fixDurationMs: number }) {
  const nextMeta: CapturePointMeta = {
    fix_duration_ms: Math.round(stats.fixDurationMs),
    sample_variance: stats.variance,
    mean_accuracy: stats.meanAccuracy,
  }
  const nextMap: MapData = {
    ...mapData.value,
    perimeter: [...mapData.value.perimeter, point],
    captureMeta: {
      perimeter: [...(mapData.value.captureMeta?.perimeter ?? []), nextMeta],
    },
  }
  const res = await fetch('/api/map', {
    method: 'POST',
    headers: apiHeaders(),
    body: JSON.stringify(nextMap),
  })
  const j = await res.json().catch(() => ({ ok: false }))
  if (!res.ok || !j.ok) {
    throw new Error('save_failed')
  }
  mapData.value = nextMap
  captureLastVariance.value = stats.variance
  captureLastMeanAccuracy.value = stats.meanAccuracy
  captureLastFixDurationMs.value = stats.fixDurationMs
  captureStatus.value = `Punkt ${nextMap.perimeter.length} gespeichert (${point[0].toFixed(2)}, ${point[1].toFixed(2)}).`
  capturePromptMode.value = 'result'
  capturePromptResultTone.value = 'success'
  capturePromptText.value = `Punkt ${nextMap.perimeter.length} gespeichert. Ø Genauigkeit ${stats.meanAccuracy.toFixed(3)} m, Varianz ${stats.variance.toFixed(4)} m².`
  capturePromptOpen.value = true
}

function openCaptureDecision() {
  capturePromptMode.value = 'decision'
  capturePromptOpen.value = true
}

async function runCaptureSampling() {
  capturePromptOpen.value = false
  captureBusy.value = true
  captureSamplingActive.value = true
  captureProgress.value = 0
  captureSampleCount.value = 0

  const samples: CaptureSample[] = []
  const startedAt = Date.now()

  return await new Promise<void>((resolve) => {
    const abortSampling = (message: string) => {
      finishCaptureSampling()
      captureStatus.value = message
      capturePromptMode.value = 'result'
      capturePromptResultTone.value = 'error'
      capturePromptText.value = message
      capturePromptOpen.value = true
      resolve()
    }

    const finalizeSampling = async () => {
      finishCaptureSampling()
      if (samples.length === 0) {
        captureStatus.value = 'Sampling ohne gueltige RTK-Proben beendet.'
        capturePromptMode.value = 'result'
        capturePromptResultTone.value = 'error'
        capturePromptText.value = 'Sampling ohne gueltige RTK-Proben beendet.'
        capturePromptOpen.value = true
        resolve()
        return
      }

      const meanX = samples.reduce((sum, sample) => sum + sample.x, 0) / samples.length
      const meanY = samples.reduce((sum, sample) => sum + sample.y, 0) / samples.length
      const meanAccuracy = samples.reduce((sum, sample) => sum + sample.acc, 0) / samples.length
      const variance = samples.reduce((sum, sample) => {
        const dx = sample.x - meanX
        const dy = sample.y - meanY
        return sum + dx * dx + dy * dy
      }, 0) / samples.length
      const fixDurationMs = samples.filter((sample) => sample.sol === 4).length * CAPTURE_SAMPLE_INTERVAL_MS

      try {
        await persistCurrentGpsPoint([meanX, meanY], { variance, meanAccuracy, fixDurationMs })
      } catch {
        captureStatus.value = 'GPS-Punkt konnte nach dem Sampling nicht gespeichert werden.'
        capturePromptMode.value = 'result'
        capturePromptResultTone.value = 'error'
        capturePromptText.value = 'GPS-Punkt konnte nach dem Sampling nicht gespeichert werden.'
        capturePromptOpen.value = true
      }
      resolve()
    }

    const tick = () => {
      if (!hasRtkSignal.value) {
        abortSampling('RTK-Signal waehrend der Aufnahme verloren. Punkt wurde nicht gespeichert.')
        return
      }

      samples.push({
        x: telemetry.value.x,
        y: telemetry.value.y,
        acc: telemetry.value.gps_acc,
        sol: telemetry.value.gps_sol,
      })
      captureSampleCount.value = samples.length

      const elapsed = Date.now() - startedAt
      captureProgress.value = Math.min(100, Math.round((elapsed / CAPTURE_SAMPLE_WINDOW_MS) * 100))
      captureStatus.value = `Sampling laeuft: ${captureProgress.value}% (${samples.length} Proben).`

      if (elapsed >= CAPTURE_SAMPLE_WINDOW_MS) {
        void finalizeSampling()
      }
    }

    tick()
    captureTimer = setInterval(tick, CAPTURE_SAMPLE_INTERVAL_MS)
  })
}

async function captureCurrentGpsPoint() {
  if (captureBusy.value) return
  if (!captureArmed.value) {
    captureStatus.value = 'Bitte zuerst "Neue Karte" starten.'
    return
  }
  if (hasRtkFix.value) {
    await runCaptureSampling()
    return
  }
  openCaptureDecision()
}

async function confirmCapturePrompt() {
  if (capturePromptMode.value === 'result') {
    capturePromptOpen.value = false
    return
  }
  if (!capturePromptCanConfirm.value || captureBusy.value) return

  await runCaptureSampling()
}

function cancelCapturePrompt() {
  capturePromptOpen.value = false
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
      :perimeter="mapData.perimeter"
      :capture-armed="captureArmed"
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

      <div class="sr-capture">
        <div class="sr-capture-row">
          <button
            class="sr-btn sr-btn-map"
            :disabled="!connected || captureBusy"
            @click="startNewMapCapture"
            title="Leere Karte anlegen und Punktaufnahme aktivieren">
            Neue Karte
          </button>
          <button
            class="sr-btn sr-btn-capture"
            :disabled="!canStartCaptureSave"
            @click="captureCurrentGpsPoint"
            title="Aktuellen GPS-Punkt als Perimeter-Punkt speichern">
            GPS-Punkt speichern
          </button>
        </div>
        <div class="sr-capture-meta">
          <span :class="rtkQualityClass">{{ rtkQualityLabel }}</span>
          <span class="sr-capture-acc">Genauigkeit: {{ rtkAccuracyLabel }}</span>
          <span class="sr-capture-count">Perimeter: {{ mapData.perimeter.length }}</span>
        </div>
        <div class="sr-capture-progress">
          <div class="sr-capture-progress-track">
            <div
              class="sr-capture-progress-fill"
              :style="{ width: `${captureProgress}%` }" />
          </div>
          <span class="sr-capture-progress-text">{{ captureProgressLabel }}</span>
        </div>
        <div class="sr-capture-status">
          {{ captureStatus || 'Für die manuelle Grenzaufnahme zuerst eine neue Karte starten.' }}
        </div>
        <div v-if="captureStatsLabel" class="sr-capture-stats">
          {{ captureStatsLabel }}
        </div>
        <div
          v-if="capturePromptOpen"
          class="sr-capture-notice"
          :class="`sr-capture-notice-${capturePromptTone}`">
          <div class="sr-capture-notice-title">{{ capturePromptTitle }}</div>
          <div class="sr-capture-notice-text">{{ capturePromptMessage }}</div>
          <div class="sr-capture-notice-actions">
            <button
              class="sr-capture-notice-ok"
              :disabled="!capturePromptCanConfirm || captureBusy"
              @click="confirmCapturePrompt">
              {{ captureBusy ? 'Speichere...' : capturePromptOkLabel }}
            </button>
            <button
              v-if="capturePromptMode === 'decision'"
              class="sr-capture-notice-cancel"
              :disabled="captureBusy"
              @click="cancelCapturePrompt">
              Abbrechen
            </button>
          </div>
        </div>
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

.sr-capture {
  border-top: 1px solid rgba(30,58,95,0.7);
  padding-top: 10px;
  display: flex;
  flex-direction: column;
  gap: 7px;
}
.sr-capture-row {
  display: flex;
  gap: 6px;
}
.sr-btn-map {
  background: #1d4ed8 !important; border-color: #60a5fa;
  color: #dbeafe !important;
}
.sr-btn-capture {
  background: #166534 !important; border-color: #4ade80;
  color: #dcfce7 !important;
}
.sr-capture-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  font-size: 11px;
}
.sr-fix-ok {
  color: #86efac !important;
}
.sr-fix-float {
  color: #fcd34d !important;
}
.sr-fix-bad {
  color: #fca5a5 !important;
}
.sr-capture-acc {
  color: #bfdbfe !important;
}
.sr-capture-count {
  color: #cbd5e1 !important;
}
.sr-capture-progress {
  display: flex;
  align-items: center;
  gap: 8px;
}
.sr-capture-progress-track {
  flex: 1;
  height: 7px;
  background: #172033;
  border-radius: 999px;
  overflow: hidden;
  border: 1px solid rgba(96, 165, 250, 0.18);
}
.sr-capture-progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #38bdf8, #4ade80);
  transition: width 0.18s linear;
}
.sr-capture-progress-text {
  min-width: 88px;
  text-align: right;
  font-size: 11px;
  color: #dbeafe !important;
}
.sr-capture-status {
  font-size: 11px;
  line-height: 1.35;
  color: #93c5fd !important;
  min-height: 30px;
}
.sr-capture-stats {
  font-size: 11px;
  line-height: 1.35;
  color: #bfdbfe !important;
}
.sr-capture-notice {
  border: 1px solid;
  border-radius: 9px;
  padding: 9px 10px;
  display: flex;
  flex-direction: column;
  gap: 7px;
}
.sr-capture-notice-success {
  background: rgba(20, 83, 45, 0.22);
  border-color: rgba(74, 222, 128, 0.6);
}
.sr-capture-notice-warning {
  background: rgba(146, 64, 14, 0.22);
  border-color: rgba(251, 191, 36, 0.65);
}
.sr-capture-notice-error {
  background: rgba(127, 29, 29, 0.22);
  border-color: rgba(248, 113, 113, 0.65);
}
.sr-capture-notice-title {
  font-size: 11px;
  font-weight: 700;
  color: #e2e8f0 !important;
}
.sr-capture-notice-text {
  font-size: 11px;
  line-height: 1.35;
  color: #dbeafe !important;
}
.sr-capture-notice-actions {
  display: flex;
  gap: 6px;
}
.sr-capture-notice-ok,
.sr-capture-notice-cancel {
  flex: 1;
  padding: 7px 8px;
  border-radius: 7px;
  border: 1px solid;
  font-size: 11px;
  font-weight: 600;
  cursor: pointer;
  font-family: sans-serif;
}
.sr-capture-notice-ok {
  background: #0f172a !important;
  border-color: #60a5fa;
  color: #dbeafe !important;
}
.sr-capture-notice-cancel {
  background: rgba(15, 23, 42, 0.75) !important;
  border-color: #475569;
  color: #cbd5e1 !important;
}
.sr-capture-notice-ok:disabled,
.sr-capture-notice-cancel:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

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
