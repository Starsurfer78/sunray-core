<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from 'vue'
import { useHistoryBackend } from '../composables/useHistoryBackend'

const { sessions, summary, loading, error, refreshHistoryData } = useHistoryBackend()

const chartCanvas = ref<HTMLCanvasElement | null>(null)

const completedSessions = computed(() =>
  sessions.value.filter((session) => session.ended_at_ms != null)
)

const longestSession = computed(() =>
  completedSessions.value.reduce((best, session) =>
    (!best || session.duration_ms > best.duration_ms) ? session : best,
  null as typeof completedSessions.value[number] | null)
)

const last10 = computed(() => [...completedSessions.value].slice(0, 10).reverse())

function fmtHm(ms: number) {
  const h = Math.floor(ms / 3_600_000)
  const m = Math.floor((ms % 3_600_000) / 60_000)
  if (h > 0) return `${h}h ${m.toString().padStart(2, '0')}m`
  return `${m}m`
}

function fmtDist(m: number) {
  return m >= 1000 ? `${(m / 1000).toFixed(2)} km` : `${Math.round(m)} m`
}

function fmtDate(ms: number) {
  return new Date(ms).toLocaleDateString('de-DE', {
    day: '2-digit', month: '2-digit', year: 'numeric',
  })
}

function drawChart(canvas: HTMLCanvasElement) {
  const ctx = canvas.getContext('2d')
  if (!ctx) return
  const W = canvas.width
  const H = canvas.height
  ctx.clearRect(0, 0, W, H)

  if (last10.value.length === 0) {
    ctx.fillStyle = '#070d18'
    ctx.fillRect(0, 0, W, H)
    ctx.fillStyle = '#4b6a8a'
    ctx.font = '12px sans-serif'
    ctx.textAlign = 'center'
    ctx.fillText('Noch keine beendeten Sessions', W / 2, H / 2 + 4)
    return
  }

  const maxMs = Math.max(...last10.value.map(s => s.duration_ms), 1)
  const padL = 10
  const padR = 10
  const padT = 10
  const padB = 28
  const slot = (W - padL - padR) / last10.value.length

  ctx.fillStyle = '#070d18'
  ctx.fillRect(0, 0, W, H)

  last10.value.forEach((session, index) => {
    const ratio = session.duration_ms / maxMs
    const barH = Math.max(4, (H - padT - padB) * ratio)
    const barW = Math.max(14, slot * 0.7)
    const x = padL + index * slot + (slot - barW) / 2
    const y = H - padB - barH

    const grad = ctx.createLinearGradient(0, y, 0, y + barH)
    grad.addColorStop(0, '#38bdf8')
    grad.addColorStop(1, '#2563eb')
    ctx.fillStyle = grad
    ctx.beginPath()
    ctx.roundRect(x, y, barW, barH, 4)
    ctx.fill()

    ctx.fillStyle = '#93c5fd'
    ctx.font = '10px sans-serif'
    ctx.textAlign = 'center'
    ctx.fillText(fmtHm(session.duration_ms), x + barW / 2, y - 3)

    ctx.fillStyle = '#4b6a8a'
    const label = new Date(session.started_at_ms).toLocaleDateString('de-DE', { day: '2-digit', month: '2-digit' })
    ctx.fillText(label, x + barW / 2, H - 10)
  })
}

watch([chartCanvas, last10], async () => {
  await nextTick()
  if (chartCanvas.value) drawChart(chartCanvas.value)
})

onMounted(async () => {
  await refreshHistoryData()
  await nextTick()
  if (chartCanvas.value) drawChart(chartCanvas.value)
})
</script>

<template>
  <div class="stats-wrap">
    <div class="stats-header">
      <div>
        <h2 class="stats-title">Statistiken</h2>
        <p class="stats-sub">Zentrale Kennzahlen direkt aus dem History-Backend.</p>
      </div>
      <button class="stats-btn" @click="refreshHistoryData">Aktualisieren</button>
    </div>

    <div v-if="loading" class="stats-state">Lade Statistik…</div>
    <div v-else-if="error" class="stats-state error">{{ error }}</div>

    <template v-else-if="summary">
      <div class="stats-cards">
        <div class="stats-card">
          <div class="stats-value">{{ summary.sessions_total }}</div>
          <div class="stats-label">Sessions gesamt</div>
        </div>
        <div class="stats-card">
          <div class="stats-value">{{ fmtHm(summary.mowing_duration_ms_total) }}</div>
          <div class="stats-label">Gesamtzeit</div>
        </div>
        <div class="stats-card">
          <div class="stats-value">{{ fmtDist(summary.mowing_distance_m_total) }}</div>
          <div class="stats-label">Gesamtstrecke</div>
        </div>
        <div class="stats-card">
          <div class="stats-value">
            {{ summary.sessions_completed > 0 ? fmtHm(summary.mowing_duration_ms_total / summary.sessions_completed) : '—' }}
          </div>
          <div class="stats-label">Ø Session</div>
        </div>
      </div>

      <div class="stats-panel">
        <div class="stats-panel-head">Letzte {{ Math.min(10, completedSessions.length) }} beendeten Sessions</div>
        <canvas ref="chartCanvas" class="stats-chart" width="640" height="200" />
      </div>

      <div class="stats-grid">
        <div class="stats-panel">
          <div class="stats-panel-head">Laengste Session</div>
          <div v-if="longestSession" class="stats-list">
            <span>{{ fmtDate(longestSession.started_at_ms) }}</span>
            <strong>{{ fmtHm(longestSession.duration_ms) }}</strong>
            <span>{{ fmtDist(longestSession.distance_m) }}</span>
          </div>
          <div v-else class="stats-empty">Noch keine beendete Session vorhanden.</div>
        </div>

        <div class="stats-panel">
          <div class="stats-panel-head">Backend / Retention</div>
          <div class="stats-list stacked">
            <span>Backend bereit: {{ summary.backend_ready ? 'ja' : 'nein' }}</span>
            <span>Datenbankdatei: {{ summary.database_exists ? 'vorhanden' : 'fehlt' }}</span>
            <span>Retention Events: {{ summary.retention.max_events }}</span>
            <span>Retention Sessions: {{ summary.retention.max_sessions }}</span>
            <span>Export: {{ summary.export_enabled ? 'aktiv' : 'deaktiviert' }}</span>
          </div>
        </div>
      </div>
    </template>
  </div>
</template>

<style scoped>
.stats-wrap {
  padding: 16px;
  min-height: 100%;
  background: #0a0f1a !important;
  color: #e2e8f0;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.stats-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}

.stats-title {
  margin: 0;
  font-size: 16px;
}

.stats-sub,
.stats-label,
.stats-empty {
  color: #8aa0ba;
}

.stats-btn {
  border: 1px solid #2563eb;
  background: #2563eb !important;
  color: white !important;
  border-radius: 8px;
  padding: 8px 12px;
  cursor: pointer;
}

.stats-cards,
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
  gap: 12px;
}

.stats-card,
.stats-panel,
.stats-state {
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  border-radius: 14px;
  padding: 14px;
}

.stats-value {
  font-size: 24px;
  font-weight: 700;
  color: #60a5fa !important;
}

.stats-panel {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.stats-panel-head {
  font-size: 12px;
  color: #8aa0ba;
  text-transform: uppercase;
  letter-spacing: 0.06em;
}

.stats-chart {
  width: 100%;
  height: 200px;
  border-radius: 10px;
  background: #070d18 !important;
  border: 1px solid #19314f;
}

.stats-list {
  display: flex;
  gap: 12px;
  align-items: center;
  flex-wrap: wrap;
}

.stats-list.stacked {
  flex-direction: column;
  align-items: flex-start;
  gap: 6px;
}

.stats-state.error {
  border-color: #7f1d1d;
  background: #2a0d0d !important;
  color: #fecaca;
}
</style>
