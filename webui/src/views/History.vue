<script setup lang="ts">
import { onMounted, watch, ref, nextTick } from 'vue'
import { useSessionTracker } from '../composables/useSessionTracker'

const { sessions, recording, clearSessions, deleteSession } = useSessionTracker()

// ── Formatting helpers ────────────────────────────────────────────────────────

function fmtDate(ms: number) {
  return new Date(ms).toLocaleDateString('de-DE', {
    day: '2-digit', month: '2-digit', year: 'numeric',
  })
}
function fmtTime(ms: number) {
  return new Date(ms).toLocaleTimeString('de-DE', {
    hour: '2-digit', minute: '2-digit',
  })
}
function fmtDuration(ms: number) {
  const h = Math.floor(ms / 3_600_000)
  const m = Math.floor((ms % 3_600_000) / 60_000)
  const s = Math.floor((ms % 60_000) / 1_000)
  if (h > 0) return `${h}h ${m.toString().padStart(2,'0')}m`
  if (m > 0) return `${m}m ${s.toString().padStart(2,'0')}s`
  return `${s}s`
}
function fmtDist(m: number) {
  return m >= 1000 ? `${(m/1000).toFixed(2)} km` : `${Math.round(m)} m`
}
function fmtBatt(start: number, end: number) {
  const delta = end - start
  const sign  = delta >= 0 ? '+' : ''
  return `${start.toFixed(1)} V → ${end.toFixed(1)} V (${sign}${delta.toFixed(1)} V)`
}

// ── Mini-path canvas ──────────────────────────────────────────────────────────

function drawPath(canvas: HTMLCanvasElement, path: [number, number][]) {
  const ctx = canvas.getContext('2d')
  if (!ctx) return
  const W = canvas.width, H = canvas.height
  ctx.clearRect(0, 0, W, H)

  if (path.length < 2) {
    ctx.fillStyle = '#1e3a5f'
    ctx.fillRect(0, 0, W, H)
    ctx.fillStyle = '#4b6a8a'
    ctx.font = '10px sans-serif'
    ctx.textAlign = 'center'
    ctx.fillText('—', W / 2, H / 2 + 4)
    return
  }

  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity
  for (const [x, y] of path) {
    if (x < minX) minX = x; if (x > maxX) maxX = x
    if (y < minY) minY = y; if (y > maxY) maxY = y
  }
  const rx = maxX - minX || 1
  const ry = maxY - minY || 1
  const pad  = 8
  const scale = Math.min((W - pad * 2) / rx, (H - pad * 2) / ry)
  const ox    = (W - rx * scale) / 2
  const oy    = (H - ry * scale) / 2
  const px    = (x: number) => ox + (x - minX) * scale
  const py    = (y: number) => oy + (maxY - y) * scale  // Y-flip

  // Background
  ctx.fillStyle = '#070d18'
  ctx.fillRect(0, 0, W, H)

  // Path line
  ctx.strokeStyle = '#22d3ee'
  ctx.lineWidth   = 1.5
  ctx.lineJoin    = 'round'
  ctx.beginPath()
  ctx.moveTo(px(path[0][0]), py(path[0][1]))
  for (let i = 1; i < path.length; i++) {
    ctx.lineTo(px(path[i][0]), py(path[i][1]))
  }
  ctx.stroke()

  // Start dot (green)
  ctx.fillStyle = '#4ade80'
  ctx.beginPath()
  ctx.arc(px(path[0][0]), py(path[0][1]), 3, 0, Math.PI * 2)
  ctx.fill()

  // End dot (red)
  ctx.fillStyle = '#f87171'
  ctx.beginPath()
  ctx.arc(px(path[path.length - 1][0]), py(path[path.length - 1][1]), 3, 0, Math.PI * 2)
  ctx.fill()
}

// ── Canvas refs ───────────────────────────────────────────────────────────────

const canvasRefs = ref<Map<string, HTMLCanvasElement>>(new Map())

function registerCanvas(id: string, el: HTMLCanvasElement | null) {
  if (el) {
    canvasRefs.value.set(id, el)
    const session = sessions.value.find(s => s.id === id)
    if (session) drawPath(el, session.path)
  }
}

// Redraw when sessions change
watch(sessions, async () => {
  await nextTick()
  for (const [id, canvas] of canvasRefs.value) {
    const session = sessions.value.find(s => s.id === id)
    if (session) drawPath(canvas, session.path)
  }
})

onMounted(async () => {
  await nextTick()
  for (const [id, canvas] of canvasRefs.value) {
    const session = sessions.value.find(s => s.id === id)
    if (session) drawPath(canvas, session.path)
  }
})

// ── Confirm clear ─────────────────────────────────────────────────────────────

const confirmClear = ref(false)

function handleClear() {
  if (!confirmClear.value) { confirmClear.value = true; return }
  clearSessions()
  canvasRefs.value.clear()
  confirmClear.value = false
}
</script>

<template>
  <div class="vh-wrap">
    <!-- Header bar -->
    <div class="vh-header">
      <h2 class="vh-title">Verlauf</h2>
      <span v-if="recording" class="vh-badge-rec">● Aufzeichnung läuft</span>
      <span class="vh-count">{{ sessions.length }} Session{{ sessions.length !== 1 ? 's' : '' }}</span>
      <button
        v-if="sessions.length > 0"
        class="vh-btn-clear"
        :class="{ confirm: confirmClear }"
        @click="handleClear"
        @blur="confirmClear = false"
      >
        {{ confirmClear ? 'Wirklich löschen?' : 'Alle löschen' }}
      </button>
    </div>

    <!-- Empty state -->
    <div v-if="sessions.length === 0 && !recording" class="vh-empty">
      <div class="vh-empty-icon">📋</div>
      <p class="vh-empty-title">Noch keine Sessions</p>
      <p class="vh-empty-sub">
        Starte den Roboter im Mäh-Modus — jede Session ab 30 s wird
        automatisch gespeichert.
      </p>
    </div>

    <!-- Active session hint -->
    <div v-if="recording" class="vh-active-hint">
      <span class="vh-dot-pulse"></span>
      Session läuft — wird nach Ende gespeichert
    </div>

    <!-- Session list -->
    <div class="vh-list">
      <div
        v-for="s in sessions"
        :key="s.id"
        class="vh-card"
      >
        <!-- Mini path canvas -->
        <canvas
          class="vh-canvas"
          width="80" height="80"
          :ref="(el) => registerCanvas(s.id, el as HTMLCanvasElement)"
        />

        <!-- Session info -->
        <div class="vh-info">
          <div class="vh-info-row">
            <span class="vh-date">{{ fmtDate(s.startedAt) }}</span>
            <span class="vh-time">{{ fmtTime(s.startedAt) }} – {{ fmtTime(s.endedAt) }}</span>
          </div>
          <div class="vh-metrics">
            <span class="vh-metric"><span class="vh-ml">Dauer</span>{{ fmtDuration(s.durationMs) }}</span>
            <span class="vh-metric"><span class="vh-ml">Strecke</span>{{ fmtDist(s.distanceM) }}</span>
            <span class="vh-metric vh-metric-batt"><span class="vh-ml">Akku</span>{{ fmtBatt(s.battStart, s.battEnd) }}</span>
          </div>
        </div>

        <!-- Delete button -->
        <button class="vh-del" title="Session löschen" @click="deleteSession(s.id)">✕</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.vh-wrap {
  display: flex;
  flex-direction: column;
  gap: 0;
  padding: 16px;
  background: #0a0f1a !important;
  min-height: 100%;
  font-family: sans-serif;
}

/* ── Header ── */
.vh-header {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 14px;
  flex-wrap: wrap;
}
.vh-title {
  font-size: 15px;
  font-weight: 600;
  color: #e2e8f0 !important;
  margin: 0;
}
.vh-count {
  font-size: 11px;
  color: #4b6a8a !important;
  margin-left: auto;
}
.vh-badge-rec {
  font-size: 11px;
  color: #4ade80 !important;
  background: #052e16 !important;
  border: 1px solid #166534;
  border-radius: 12px;
  padding: 2px 8px;
  animation: blink 1.2s step-end infinite;
}
@keyframes blink { 50% { opacity: 0.4 } }

.vh-btn-clear {
  font-size: 11px;
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  color: #60a5fa !important;
  border-radius: 6px;
  padding: 4px 10px;
  cursor: pointer;
}
.vh-btn-clear.confirm {
  border-color: #dc2626 !important;
  color: #f87171 !important;
  background: #1a0505 !important;
}

/* ── Empty ── */
.vh-empty {
  text-align: center;
  padding: 48px 24px;
  color: #4b6a8a !important;
}
.vh-empty-icon { font-size: 36px; margin-bottom: 12px; }
.vh-empty-title { font-size: 14px; color: #94a3b8 !important; margin: 0 0 6px; }
.vh-empty-sub   { font-size: 12px; margin: 0; line-height: 1.6; }

/* ── Active hint ── */
.vh-active-hint {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
  color: #4ade80 !important;
  background: #052e16 !important;
  border: 1px solid #166534;
  border-radius: 8px;
  padding: 8px 12px;
  margin-bottom: 12px;
}
.vh-dot-pulse {
  width: 8px; height: 8px;
  background: #4ade80;
  border-radius: 50%;
  animation: pulse 1.4s ease-in-out infinite;
}
@keyframes pulse { 0%,100% { transform: scale(1); opacity: 1 } 50% { transform: scale(1.4); opacity: 0.6 } }

/* ── Session list ── */
.vh-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.vh-card {
  display: flex;
  align-items: center;
  gap: 12px;
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  border-radius: 10px;
  padding: 10px 12px;
}
.vh-card:hover { border-color: #2563eb; }

.vh-canvas {
  border-radius: 6px;
  border: 1px solid #1e3a5f;
  flex-shrink: 0;
  background: #070d18 !important;
}

.vh-info {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.vh-info-row {
  display: flex;
  gap: 10px;
  align-items: baseline;
  flex-wrap: wrap;
}
.vh-date {
  font-size: 13px;
  font-weight: 600;
  color: #e2e8f0 !important;
}
.vh-time {
  font-size: 11px;
  color: #60a5fa !important;
}
.vh-metrics {
  display: flex;
  gap: 14px;
  flex-wrap: wrap;
}
.vh-metric {
  font-size: 12px;
  color: #cbd5e1 !important;
  display: flex;
  gap: 5px;
  align-items: baseline;
}
.vh-ml {
  font-size: 10px;
  color: #4b6a8a !important;
  text-transform: uppercase;
  letter-spacing: 0.04em;
}
.vh-metric-batt { color: #c4b5fd !important; }

.vh-del {
  background: transparent !important;
  border: 1px solid #1e3a5f;
  color: #4b6a8a !important;
  border-radius: 5px;
  width: 24px; height: 24px;
  cursor: pointer;
  font-size: 10px;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}
.vh-del:hover {
  border-color: #dc2626 !important;
  color: #f87171 !important;
  background: #1a0505 !important;
}
</style>
