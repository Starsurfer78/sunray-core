<script setup lang="ts">
import { ref, computed } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'
import VirtualJoystick from './VirtualJoystick.vue'

const { telemetry, connected, sendCmd } = useTelemetry()

const open        = ref(true)
const joystickOn  = ref(false)

// ── Computed ──────────────────────────────────────────────────────────────────
const opLabel = computed(() => {
  const labels: Record<string, string> = {
    idle: 'Bereit',
    undocking: 'Ausparken',
    navigating_to_start: 'Zum Startpunkt',
    mowing: 'Mähend',
    docking: 'Dockend',
    charging: 'Laden',
    waiting_for_rain: 'Regenpause',
    gps_recovery: 'GPS-Warten',
    obstacle_recovery: 'Ausweichen',
    fault: 'Fehler',
  }
  return labels[telemetry.value.state_phase ?? 'idle'] ?? telemetry.value.op
})

const gpsFixLabel = computed(() => {
  if (telemetry.value.gps_sol === 4) return 'RTK-Fix'
  if (telemetry.value.gps_sol === 5) return 'RTK-Float'
  return 'Kein Fix'
})
const gpsFixColor = computed(() => {
  if (telemetry.value.gps_sol === 4) return '#4ade80'
  if (telemetry.value.gps_sol === 5) return '#fbbf24'
  return '#f87171'
})

const battPct = computed(() => {
  const pct = ((telemetry.value.battery_v - 22) / (29.4 - 22)) * 100
  return Math.max(0, Math.min(100, Math.round(pct)))
})

const headingDeg = computed(() =>
  String(Math.round(telemetry.value.heading * 180 / Math.PI)).padStart(3, '0')
)

const canStart = computed(() => connected.value && ['idle', 'charging'].includes(telemetry.value.state_phase ?? 'idle'))
const canDock  = computed(() => connected.value && ['mowing', 'navigating_to_start', 'gps_recovery', 'obstacle_recovery', 'waiting_for_rain'].includes(telemetry.value.state_phase ?? 'idle'))
const canStop  = computed(() => connected.value && !['idle', 'charging', 'fault'].includes(telemetry.value.state_phase ?? 'idle'))
const uiNoticeClass = computed(() => {
  if (telemetry.value.ui_severity === 'error') return 'sr-notice sr-notice-error'
  if (telemetry.value.ui_severity === 'warn') return 'sr-notice sr-notice-warn'
  return 'sr-notice sr-notice-info'
})
</script>

<template>
  <aside class="sr-aside">
    <!-- Collapse toggle -->
    <button class="sr-toggle" :class="{ 'sr-toggle-closed': !open }" @click="open = !open" :title="open ? 'Sidebar einklappen' : 'Sidebar ausklappen'">
      {{ open ? '›' : '‹' }}
    </button>

    <div v-show="open" class="sr-side">

      <!-- Status -->
      <div class="sr-sec">
        <div class="sr-lbl">Status</div>
        <div class="sr-stbig">{{ opLabel }}</div>
        <div class="sr-stsub">{{ telemetry.state_phase === 'mowing' ? 'Zone 1 — Hauptfläche' : '—' }}</div>
        <div v-if="telemetry.ui_message" :class="uiNoticeClass">{{ telemetry.ui_message }}</div>
        <div class="sr-pos">
          X {{ telemetry.x.toFixed(2) }} m &nbsp;
          Y {{ telemetry.y.toFixed(2) }} m &nbsp;
          {{ headingDeg }}°
        </div>
      </div>

      <!-- GPS -->
      <div class="sr-sec">
        <div class="sr-lbl">GPS</div>
        <div class="sr-gps">
          <div class="sr-gps-top">
            <div class="sr-fix">
              <span class="sr-fixdot" :style="{ background: gpsFixColor }"></span>
              <span :style="{ color: gpsFixColor }">{{ gpsFixLabel }}</span>
            </div>
            <span class="sr-sol">sol: {{ telemetry.gps_sol }}</span>
          </div>
          <div class="sr-satrow">
            <div class="sr-sat">
              <div class="sr-satlbl">Rover</div>
              <div class="sr-satval">—</div>
              <div class="sr-satsub">Satelliten</div>
            </div>
            <div class="sr-sat">
              <div class="sr-satlbl">Base</div>
              <div class="sr-satval">—</div>
              <div class="sr-satsub">Satelliten</div>
            </div>
          </div>
          <div class="sr-coords">
            {{ telemetry.gps_lat.toFixed(8) }}°N &nbsp;
            {{ telemetry.gps_lon.toFixed(8) }}°E
          </div>
        </div>
      </div>

      <!-- Akku -->
      <div class="sr-sec">
        <div class="sr-lbl">Akku</div>
        <div class="sr-sg">
          <div class="sr-sc">
            <div class="sr-slbl">Spannung</div>
            <div class="sr-sval" style="color: #60a5fa !important">{{ telemetry.battery_v.toFixed(1) }} V</div>
            <div class="sr-bar"><div class="sr-barf" :style="{ width: battPct + '%' }"></div></div>
          </div>
          <div class="sr-sc">
            <div class="sr-slbl">Ladespannung</div>
            <div class="sr-sval" style="color: #fbbf24 !important">{{ telemetry.charge_v.toFixed(1) }} V</div>
          </div>
        </div>
      </div>

      <!-- Sensoren -->
      <div class="sr-sec">
        <div class="sr-lbl">Sensoren</div>
        <div class="sr-sensg">
          <div class="sr-sens"><span :class="telemetry.bumper_l ? 'derr' : 'dok'"></span>Bumper L</div>
          <div class="sr-sens"><span :class="telemetry.bumper_r ? 'derr' : 'dok'"></span>Bumper R</div>
          <div class="sr-sens"><span class="dok"></span>Lift</div>
          <div class="sr-sens"><span :class="telemetry.motor_err ? 'derr' : 'dok'"></span>Motor</div>
        </div>
      </div>

      <!-- Coverage -->
      <div class="sr-sec">
        <div class="sr-lbl">Coverage</div>
        <div class="sr-covrow"><span>Gemäht heute</span><span class="sr-covval">0%</span></div>
        <div class="sr-covbar"><div class="sr-covfill" style="width: 0%"></div></div>
      </div>

      <!-- Steuerung -->
      <div class="sr-sec">
        <div class="sr-lbl">Steuerung</div>
        <div class="sr-ctrlrow">
          <button class="sr-cb cb-start" :disabled="!canStart" @click="sendCmd('start')">Start</button>
          <button class="sr-cb cb-pause" :disabled="!canStop"  @click="sendCmd('stop')">Stop</button>
          <button class="sr-cb cb-dock"  :disabled="!canDock"  @click="sendCmd('dock')">Dock</button>
        </div>
        <!-- Joystick toggle -->
        <button
          class="sr-joybtn"
          :class="{ active: joystickOn }"
          @click="joystickOn = !joystickOn"
          title="Manuell fahren"
        >
          🕹 Joystick
        </button>
      </div>

      <!-- Nächste Starts -->
      <div class="sr-sec">
        <div class="sr-lbl">Nächste Starts</div>
        <div class="sr-sched"><span>Mo, Di, Fr</span><span class="sr-st">09:00</span></div>
        <div class="sr-sched"><span>Sa</span><span class="sr-st">08:00</span></div>
        <div class="sr-sched"><span>Mo</span><span class="sr-st">14:00</span></div>
      </div>

    </div>

    <!-- Joystick: teleported to body, manages its own fixed position -->
    <Teleport to="body">
      <VirtualJoystick v-if="joystickOn" @close="joystickOn = false" />
    </Teleport>
  </aside>
</template>

<style scoped>
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

/* ── Aside shell ── */
.sr-aside {
  position: relative;
  flex-shrink: 0;
  display: flex;
  font-family: sans-serif;
}

/* ── Collapse toggle ── */
.sr-toggle {
  position: absolute;
  left: -14px;
  top: 50%;
  transform: translateY(-50%);
  z-index: 10;
  width: 14px;
  height: 44px;
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  border-right: none;
  color: #475569 !important;
  cursor: pointer;
  border-radius: 6px 0 0 6px;
  font-size: 14px;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
}
.sr-toggle:hover { color: #60a5fa !important; }
.sr-toggle-closed { left: -14px; border-right: 1px solid #1e3a5f; border-radius: 6px; }

/* ── Sidebar content ── */
.sr-side {
  width: 252px;
  background: #0a1020 !important;
  border-left: 1px solid #1e3a5f;
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
  overflow-y: auto;
}

.sr-sec { padding: 11px 14px; border-bottom: 1px solid #0f1829; }

.sr-lbl {
  font-size: 10px; font-weight: 500; color: #475569 !important;
  text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;
}

/* Status */
.sr-stbig { font-size: 20px; font-weight: 500; color: #60a5fa !important; }
.sr-stsub { font-size: 11px; color: #64748b !important; margin-top: 2px; }
.sr-notice {
  margin-top: 8px;
  padding: 8px 10px;
  border-radius: 8px;
  font-size: 12px;
  line-height: 1.35;
  border: 1px solid transparent;
}
.sr-notice-info {
  background: #0f2340 !important;
  border-color: #1d4ed8;
  color: #bfdbfe !important;
}
.sr-notice-warn {
  background: #2a1704 !important;
  border-color: #d97706;
  color: #fde68a !important;
}
.sr-notice-error {
  background: #2b0b0b !important;
  border-color: #dc2626;
  color: #fecaca !important;
}
.sr-pos   { font-family: monospace; font-size: 11px; color: #64748b !important; margin-top: 5px; }

/* GPS */
.sr-gps { background: #0f1829 !important; border-radius: 7px; padding: 9px 10px; border: 1px solid #1e3a5f; }
.sr-gps-top { display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px; }
.sr-fix { font-size: 13px; font-weight: 500; display: flex; align-items: center; gap: 5px; }
.sr-fixdot { width: 7px; height: 7px; border-radius: 50%; flex-shrink: 0; }
.sr-sol { font-size: 10px; color: #475569 !important; font-family: monospace; background: #0a1020; padding: 2px 6px; border-radius: 4px; }
.sr-satrow { display: grid; grid-template-columns: 1fr 1fr; gap: 6px; margin-bottom: 7px; }
.sr-sat { background: #070d18 !important; border-radius: 5px; padding: 7px 9px; border: 1px solid #1e3a5f; }
.sr-satlbl { font-size: 10px; color: #64748b !important; margin-bottom: 3px; }
.sr-satval { font-size: 18px; font-weight: 500; color: #60a5fa !important; font-family: monospace; }
.sr-satsub { font-size: 10px; color: #475569 !important; }
.sr-coords { font-size: 10px; color: #475569 !important; font-family: monospace; line-height: 1.7; }

/* Battery */
.sr-sg   { display: grid; grid-template-columns: 1fr 1fr; gap: 6px; }
.sr-sc   { background: #0f1829 !important; border-radius: 6px; padding: 8px 10px; border: 1px solid #1a2a40; }
.sr-slbl { font-size: 10px; color: #475569 !important; margin-bottom: 3px; }
.sr-sval { font-size: 15px; font-weight: 500; }
.sr-bar  { height: 4px; background: #0a1020; border-radius: 2px; overflow: hidden; margin-top: 5px; }
.sr-barf { height: 100%; background: #2563eb; border-radius: 2px; transition: width 0.4s; }

/* Sensors */
.sr-sensg { display: grid; grid-template-columns: 1fr 1fr; gap: 5px; }
.sr-sens {
  background: #0f1829 !important; border-radius: 5px; padding: 6px 8px;
  font-size: 11px; display: flex; align-items: center; gap: 6px;
  color: #94a3b8 !important; border: 1px solid #1a2a40;
}
.dok  { width: 7px; height: 7px; border-radius: 50%; background: #166534; border: 1.5px solid #4ade80; flex-shrink: 0; }
.derr { width: 7px; height: 7px; border-radius: 50%; background: #450a0a; border: 1.5px solid #f87171; flex-shrink: 0; }

/* Coverage */
.sr-covrow  { display: flex; justify-content: space-between; font-size: 12px; margin-bottom: 6px; color: #64748b !important; }
.sr-covval  { color: #22d3ee !important; font-weight: 500; }
.sr-covbar  { height: 4px; background: #0a1020; border-radius: 2px; overflow: hidden; }
.sr-covfill { height: 100%; background: #0891b2; border-radius: 2px; }

/* Controls */
.sr-ctrlrow { display: flex; gap: 6px; }
.sr-cb {
  flex: 1; padding: 8px 4px; border-radius: 7px; font-size: 12px;
  font-weight: 500; cursor: pointer; text-align: center; font-family: sans-serif;
}
.sr-cb:disabled { opacity: 0.35; cursor: not-allowed; }
.cb-start { background: #0c1a3a !important; border: 1px solid #2563eb; color: #93c5fd !important; }
.cb-pause { background: #0f1829 !important; border: 1px solid #1e3a5f; color: #64748b !important; }
.cb-dock  { background: #1c1200 !important; border: 1px solid #d97706; color: #fbbf24 !important; }

.sr-joybtn {
  margin-top: 8px; width: 100%; padding: 7px; border-radius: 7px;
  background: #0f1829 !important; border: 1px solid #1e3a5f;
  color: #64748b !important; font-size: 12px; cursor: pointer; font-family: sans-serif;
}
.sr-joybtn:hover { border-color: #3b82f6; color: #93c5fd !important; }
.sr-joybtn.active { background: #0c1a3a !important; border-color: #3b82f6; color: #93c5fd !important; }

/* Schedule */
.sr-sched {
  font-size: 11px; color: #64748b !important; padding: 5px 0;
  border-bottom: 1px solid #0f1829; display: flex; justify-content: space-between;
}
.sr-sched:last-child { border-bottom: none; }
.sr-st { color: #60a5fa !important; font-family: monospace; }

/* VirtualJoystick manages its own position via :style — no wrapper needed */
</style>
