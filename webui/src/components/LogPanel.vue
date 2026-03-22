<script setup lang="ts">
import { ref, watch, nextTick } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'

const { telemetry, logs, clearLogs } = useTelemetry()

const open = ref(false)
const tab  = ref<'log' | 'gps'>('log')
const logEl = ref<HTMLDivElement | null>(null)

// Auto-scroll to top (newest first) when log grows and panel is open
watch(logs, () => {
  if (open.value && tab.value === 'log') {
    nextTick(() => { if (logEl.value) logEl.value.scrollTop = 0 })
  }
})

const gpsFixLabel = (sol: number) => sol === 4 ? 'RTK-Fix' : sol === 5 ? 'RTK-Float' : 'Kein Fix'
const gpsFixColor = (sol: number) => sol === 4 ? '#4ade80' : sol === 5 ? '#fbbf24' : '#f87171'
</script>

<template>
  <div class="lp" :class="{ 'lp-open': open }">

    <!-- Toggle bar -->
    <div class="lp-bar">
      <button :class="['lp-tab', tab === 'log' && 'active']" @click="tab = 'log'; open = true">
        Live Log
        <span v-if="logs.length" class="lp-badge">{{ logs.length }}</span>
      </button>
      <button :class="['lp-tab', tab === 'gps' && 'active']" @click="tab = 'gps'; open = true">
        GPS Details
      </button>
      <div class="lp-spacer" />
      <button v-if="open && tab === 'log' && logs.length" class="lp-clearbtn" @click="clearLogs()">
        Leeren
      </button>
      <button class="lp-toggle" :title="open ? 'Panel einklappen' : 'Panel ausklappen'" @click="open = !open">
        {{ open ? '▼' : '▲' }}
      </button>
    </div>

    <!-- Content -->
    <div v-if="open" class="lp-content">

      <!-- Live Log -->
      <div v-if="tab === 'log'" ref="logEl" class="lp-log">
        <div v-if="logs.length === 0" class="lp-empty">Keine Log-Einträge — Verbindung zum Roboter herstellen.</div>
        <div v-for="(line, i) in logs" :key="i" class="lp-line">{{ line }}</div>
      </div>

      <!-- GPS Details -->
      <div v-else class="lp-gps">
        <div class="lp-grow">
          <div class="lp-gcell">
            <div class="lp-glbl">Fix</div>
            <div class="lp-gval" :style="{ color: gpsFixColor(telemetry.gps_sol) }">
              {{ gpsFixLabel(telemetry.gps_sol) }}
            </div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">Solution</div>
            <div class="lp-gval">{{ telemetry.gps_sol }}</div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">Status</div>
            <div class="lp-gval lp-gsmall">{{ telemetry.gps_text || '—' }}</div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">Breitengrad</div>
            <div class="lp-gval lp-gmono">{{ telemetry.gps_lat.toFixed(8) }}°</div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">Längengrad</div>
            <div class="lp-gval lp-gmono">{{ telemetry.gps_lon.toFixed(8) }}°</div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">X lokal</div>
            <div class="lp-gval lp-gmono">{{ telemetry.x.toFixed(3) }} m</div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">Y lokal</div>
            <div class="lp-gval lp-gmono">{{ telemetry.y.toFixed(3) }} m</div>
          </div>
          <div class="lp-gcell">
            <div class="lp-glbl">Richtung</div>
            <div class="lp-gval lp-gmono">{{ (telemetry.heading * 180 / Math.PI).toFixed(1) }}°</div>
          </div>
        </div>
      </div>

    </div>
  </div>
</template>

<style scoped>
*, *::before, *::after { box-sizing: border-box; }

.lp {
  flex-shrink: 0;
  background: #070d18 !important;
  border-top: 1px solid #1e3a5f;
  display: flex;
  flex-direction: column;
  font-family: sans-serif;
}

/* ── Toggle bar ── */
.lp-bar {
  display: flex;
  align-items: center;
  height: 32px;
  padding: 0 8px;
  gap: 2px;
  flex-shrink: 0;
}

.lp-tab {
  display: flex;
  align-items: center;
  gap: 5px;
  padding: 4px 10px;
  font-size: 11px;
  border-radius: 5px;
  cursor: pointer;
  background: transparent;
  border: none;
  color: #475569 !important;
  font-family: sans-serif;
}
.lp-tab:hover { color: #94a3b8 !important; }
.lp-tab.active { background: #0f1829 !important; color: #93c5fd !important; }

.lp-badge {
  font-size: 9px;
  background: #1e3a5f;
  color: #60a5fa !important;
  border-radius: 8px;
  padding: 1px 5px;
  font-family: monospace;
}

.lp-spacer { flex: 1; }

.lp-clearbtn {
  font-size: 10px; padding: 2px 8px; border-radius: 4px; cursor: pointer;
  background: transparent; border: 1px solid #1e3a5f;
  color: #475569 !important; font-family: sans-serif;
}
.lp-clearbtn:hover { border-color: #ef4444; color: #f87171 !important; }

.lp-toggle {
  padding: 4px 8px; font-size: 10px; border-radius: 4px; cursor: pointer;
  background: transparent; border: 1px solid #1e3a5f;
  color: #475569 !important; font-family: sans-serif;
}
.lp-toggle:hover { color: #60a5fa !important; border-color: #3b82f6; }

/* ── Content ── */
.lp-content {
  height: 180px;
  min-height: 0;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

/* Live Log */
.lp-log {
  flex: 1;
  overflow-y: auto;
  padding: 4px 8px;
}
.lp-empty { font-size: 11px; color: #334155 !important; padding: 8px 0; }
.lp-line  {
  font-size: 11px;
  font-family: monospace;
  color: #64748b !important;
  padding: 1px 0;
  border-bottom: 1px solid #0a1020;
  white-space: pre-wrap;
  word-break: break-all;
}
.lp-line:first-child { color: #94a3b8 !important; }  /* newest line highlighted */

/* GPS Details */
.lp-gps { flex: 1; overflow-y: auto; padding: 8px; }
.lp-grow {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 6px;
}
.lp-gcell {
  background: #0f1829 !important;
  border: 1px solid #1a2a40;
  border-radius: 6px;
  padding: 6px 8px;
}
.lp-glbl { font-size: 9px; color: #475569 !important; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 3px; }
.lp-gval { font-size: 13px; font-weight: 500; color: #93c5fd !important; }
.lp-gsmall { font-size: 11px; }
.lp-gmono { font-family: monospace; }
</style>
