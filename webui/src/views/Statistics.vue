<script setup lang="ts">
import { computed, onMounted, ref, watch, nextTick } from 'vue'
import { useSessionTracker } from '../composables/useSessionTracker'

const { sessions } = useSessionTracker()

// ── Aggregate metrics ─────────────────────────────────────────────────────────

const totalSessions = computed(() => sessions.value.length)

const totalMs = computed(() =>
  sessions.value.reduce((acc, s) => acc + s.durationMs, 0)
)

const totalDistM = computed(() =>
  sessions.value.reduce((acc, s) => acc + s.distanceM, 0)
)

const avgDurationMs = computed(() =>
  totalSessions.value > 0 ? totalMs.value / totalSessions.value : 0
)

const longestSession = computed(() =>
  sessions.value.reduce((best, s) =>
    (!best || s.durationMs > best.durationMs) ? s : best
  , null as typeof sessions.value[0] | null)
)

// ── Formatting ────────────────────────────────────────────────────────────────

function fmtHm(ms: number) {
  const h = Math.floor(ms / 3_600_000)
  const m = Math.floor((ms % 3_600_000) / 60_000)
  return h > 0 ? `${h}h ${m.toString().padStart(2,'0')}m` : `${m}m`
}

function fmtDist(m: number) {
  return m >= 1000 ? `${(m / 1000).toFixed(2)} km` : `${Math.round(m)} m`
}

function fmtDate(ms: number) {
  return new Date(ms).toLocaleDateString('de-DE', {
    day: '2-digit', month: '2-digit', year: 'numeric',
  })
}

// ── Bar chart ─────────────────────────────────────────────────────────────────

const chartCanvas = ref<HTMLCanvasElement | null>(null)

const last10 = computed(() => [...sessions.value].slice(0, 10).reverse())

function drawChart(canvas: HTMLCanvasElement) {
  const data = last10.value
  const ctx  = canvas.getContext('2d')
  if (!ctx) return
  const W = canvas.width, H = canvas.height
  ctx.clearRect(0, 0, W, H)

  if (data.length === 0) {
    ctx.fillStyle = '#0f1829'
    ctx.fillRect(0, 0, W, H)
    ctx.fillStyle = '#4b6a8a'
    ctx.font = '12px sans-serif'
    ctx.textAlign = 'center'
    ctx.fillText('Keine Sessions', W / 2, H / 2 + 4)
    return
  }

  const maxMs  = Math.max(...data.map(s => s.durationMs))
  const n      = data.length
  const padL   = 8, padR = 8, padT = 8, padB = 28
  const barW   = Math.floor((W - padL - padR) / n)
  const barGap = Math.max(2, Math.floor(barW * 0.2))
  const bW     = barW - barGap

  ctx.fillStyle = '#070d18'
  ctx.fillRect(0, 0, W, H)

  data.forEach((s, i) => {
    const ratio = s.durationMs / (maxMs || 1)
    const barH  = Math.max(4, Math.floor((H - padT - padB) * ratio))
    const bx    = padL + i * barW + Math.floor(barGap / 2)
    const by    = H - padB - barH

    // Bar gradient
    const grad = ctx.createLinearGradient(0, by, 0, by + barH)
    grad.addColorStop(0, '#3b82f6')
    grad.addColorStop(1, '#1d4ed8')
    ctx.fillStyle = grad
    ctx.beginPath()
    ctx.roundRect(bx, by, bW, barH, 3)
    ctx.fill()

    // Date label below bar
    ctx.fillStyle = '#4b6a8a'
    ctx.font = '9px sans-serif'
    ctx.textAlign = 'center'
    const label = new Date(s.startedAt).toLocaleDateString('de-DE', { day: '2-digit', month: '2-digit' })
    ctx.fillText(label, bx + bW / 2, H - padB + 13)

    // Duration above bar
    ctx.fillStyle = '#93c5fd'
    ctx.font = '9px sans-serif'
    ctx.textAlign = 'center'
    ctx.fillText(fmtHm(s.durationMs), bx + bW / 2, by - 3)
  })
}

watch([chartCanvas, last10], async () => {
  await nextTick()
  if (chartCanvas.value) drawChart(chartCanvas.value)
}, { immediate: false })

onMounted(async () => {
  await nextTick()
  if (chartCanvas.value) drawChart(chartCanvas.value)
})
</script>

<template>
  <div class="vs-wrap">
    <h2 class="vs-title">Statistiken</h2>

    <!-- Summary cards -->
    <div class="vs-cards">
      <div class="vs-card">
        <div class="vs-card-val">{{ totalSessions }}</div>
        <div class="vs-card-lbl">Sessions</div>
      </div>
      <div class="vs-card">
        <div class="vs-card-val">{{ fmtHm(totalMs) }}</div>
        <div class="vs-card-lbl">Gesamtzeit</div>
      </div>
      <div class="vs-card">
        <div class="vs-card-val">{{ fmtDist(totalDistM) }}</div>
        <div class="vs-card-lbl">Gesamtstrecke</div>
      </div>
      <div class="vs-card">
        <div class="vs-card-val">{{ totalSessions > 0 ? fmtHm(avgDurationMs) : '—' }}</div>
        <div class="vs-card-lbl">Ø Session</div>
      </div>
    </div>

    <!-- Bar chart — last 10 sessions -->
    <div class="vs-section">
      <div class="vs-section-hdr">Letzte {{ Math.min(10, totalSessions) }} Sessions — Dauer</div>
      <canvas
        ref="chartCanvas"
        class="vs-chart"
        width="600" height="180"
      />
    </div>

    <!-- Longest session -->
    <div v-if="longestSession" class="vs-section">
      <div class="vs-section-hdr">Längste Session</div>
      <div class="vs-best">
        <span class="vs-best-date">{{ fmtDate(longestSession.startedAt) }}</span>
        <span class="vs-best-dur">{{ fmtHm(longestSession.durationMs) }}</span>
        <span class="vs-best-dist">{{ fmtDist(longestSession.distanceM) }}</span>
      </div>
    </div>

    <!-- Empty state -->
    <div v-if="totalSessions === 0" class="vs-empty">
      <div class="vs-empty-icon">📊</div>
      <p class="vs-empty-title">Noch keine Daten</p>
      <p class="vs-empty-sub">Statistiken werden aus aufgezeichneten Sessions berechnet.</p>
    </div>
  </div>
</template>

<style scoped>
.vs-wrap {
  padding: 16px;
  background: #0a0f1a !important;
  min-height: 100%;
  font-family: sans-serif;
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.vs-title {
  font-size: 15px;
  font-weight: 600;
  color: #e2e8f0 !important;
  margin: 0;
}

/* ── Summary cards ── */
.vs-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
  gap: 10px;
}
.vs-card {
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  border-radius: 10px;
  padding: 14px 16px;
  text-align: center;
}
.vs-card-val {
  font-size: 22px;
  font-weight: 700;
  color: #60a5fa !important;
  line-height: 1.1;
}
.vs-card-lbl {
  font-size: 10px;
  color: #4b6a8a !important;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  margin-top: 4px;
}

/* ── Sections ── */
.vs-section {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.vs-section-hdr {
  font-size: 11px;
  color: #4b6a8a !important;
  text-transform: uppercase;
  letter-spacing: 0.06em;
}

/* ── Bar chart ── */
.vs-chart {
  width: 100%;
  height: 180px;
  border-radius: 8px;
  border: 1px solid #1e3a5f;
  background: #070d18 !important;
  display: block;
}

/* ── Best session ── */
.vs-best {
  display: flex;
  align-items: center;
  gap: 14px;
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  border-radius: 10px;
  padding: 12px 16px;
  flex-wrap: wrap;
}
.vs-best-date {
  font-size: 13px;
  font-weight: 600;
  color: #e2e8f0 !important;
}
.vs-best-dur {
  font-size: 13px;
  color: #60a5fa !important;
  font-weight: 600;
}
.vs-best-dist {
  font-size: 12px;
  color: #94a3b8 !important;
}

/* ── Empty ── */
.vs-empty {
  text-align: center;
  padding: 32px 24px;
}
.vs-empty-icon  { font-size: 36px; margin-bottom: 12px; }
.vs-empty-title { font-size: 14px; color: #94a3b8 !important; margin: 0 0 6px; }
.vs-empty-sub   { font-size: 12px; color: #4b6a8a !important; margin: 0; }
</style>
