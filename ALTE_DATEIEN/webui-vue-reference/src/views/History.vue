<script setup lang="ts">
import { computed, ref } from 'vue'
import { useHistoryBackend } from '../composables/useHistoryBackend'
import { useTelemetry } from '../composables/useTelemetry'

const { events, sessions, summary, loading, error, lastUpdated, refreshHistoryData } = useHistoryBackend()
const { telemetry } = useTelemetry()

const timeFilter = ref<'all' | '24h' | '7d' | '30d'>('7d')
const typeFilter = ref('all')
const stateFilter = ref('all')
const onlyMissionEnds = ref(false)

function cutoffMs() {
  const now = Date.now()
  if (timeFilter.value === '24h') return now - 24 * 60 * 60 * 1000
  if (timeFilter.value === '7d') return now - 7 * 24 * 60 * 60 * 1000
  if (timeFilter.value === '30d') return now - 30 * 24 * 60 * 60 * 1000
  return 0
}

const eventTypes = computed(() =>
  [...new Set(events.value.map(e => e.event_type).filter(Boolean))].sort()
)

const statePhases = computed(() =>
  [...new Set([
    ...events.value.map(e => e.state_phase),
    ...sessions.value.map(s => mapSessionState(s)),
  ].filter(Boolean))].sort()
)

function mapSessionState(session: { metadata?: Record<string, unknown> }) {
  const raw = session.metadata?.state_phase
  return typeof raw === 'string' && raw ? raw : 'mowing'
}

const filteredEvents = computed(() => {
  const cutoff = cutoffMs()
  return events.value.filter((event) => {
    if (cutoff > 0 && event.ts_ms < cutoff) return false
    if (typeFilter.value !== 'all' && event.event_type !== typeFilter.value) return false
    if (stateFilter.value !== 'all' && event.state_phase !== stateFilter.value) return false
    if (onlyMissionEnds.value) {
      return event.event_type === 'state_transition' && event.event_reason !== 'mission_active'
    }
    return true
  })
})

const filteredSessions = computed(() => {
  const cutoff = cutoffMs()
  return sessions.value.filter((session) => {
    if (cutoff > 0 && session.started_at_ms < cutoff) return false
    if (stateFilter.value !== 'all' && mapSessionState(session) !== stateFilter.value) return false
    if (onlyMissionEnds.value && !session.ended_at_ms) return false
    return true
  })
})

const recording = computed(() => telemetry.value.state_phase === 'mowing')
const exportUrl = computed(() => summary.value?.export_enabled ? '/api/history/export' : '')

function fmtDateTime(ms: number) {
  return new Date(ms).toLocaleString('de-DE', {
    day: '2-digit', month: '2-digit', year: 'numeric',
    hour: '2-digit', minute: '2-digit',
  })
}

function fmtDuration(ms: number) {
  const h = Math.floor(ms / 3_600_000)
  const m = Math.floor((ms % 3_600_000) / 60_000)
  const s = Math.floor((ms % 60_000) / 1_000)
  if (h > 0) return `${h}h ${m.toString().padStart(2, '0')}m`
  if (m > 0) return `${m}m ${s.toString().padStart(2, '0')}s`
  return `${s}s`
}

function fmtDist(m: number) {
  return m >= 1000 ? `${(m / 1000).toFixed(2)} km` : `${Math.round(m)} m`
}

function fmtBattery(start: number, end: number) {
  const delta = end - start
  const sign = delta >= 0 ? '+' : ''
  return `${start.toFixed(1)} V → ${end.toFixed(1)} V (${sign}${delta.toFixed(1)} V)`
}

function lastUpdatedText() {
  if (!lastUpdated.value) return 'noch nicht geladen'
  return fmtDateTime(lastUpdated.value)
}
</script>

<template>
  <div class="history-wrap">
    <div class="history-header">
      <div>
        <h2 class="history-title">Verlauf</h2>
        <p class="history-sub">
          Backend-gestuetzte Ereignisse und Mäh-Sessions.
        </p>
      </div>
      <div class="history-actions">
        <span v-if="recording" class="history-badge">Session laeuft</span>
        <a v-if="exportUrl" class="history-btn secondary" :href="exportUrl">DB exportieren</a>
        <button class="history-btn" @click="refreshHistoryData">Aktualisieren</button>
      </div>
    </div>

    <div class="history-toolbar">
      <label>
        Zeitraum
        <select v-model="timeFilter">
          <option value="all">Gesamt</option>
          <option value="24h">24 Stunden</option>
          <option value="7d">7 Tage</option>
          <option value="30d">30 Tage</option>
        </select>
      </label>
      <label>
        Ereignistyp
        <select v-model="typeFilter">
          <option value="all">Alle</option>
          <option v-for="type in eventTypes" :key="type" :value="type">{{ type }}</option>
        </select>
      </label>
      <label>
        Zustand
        <select v-model="stateFilter">
          <option value="all">Alle</option>
          <option v-for="phase in statePhases" :key="phase" :value="phase">{{ phase }}</option>
        </select>
      </label>
      <label class="history-check">
        <input v-model="onlyMissionEnds" type="checkbox">
        Nur Missionsenden
      </label>
    </div>

    <div class="history-meta">
      <span>Letztes Update: {{ lastUpdatedText() }}</span>
      <span v-if="summary">Sessions gesamt: {{ summary.sessions_total }}</span>
      <span v-if="summary">Events gesamt: {{ summary.events_total }}</span>
      <span v-if="summary">Backend: {{ summary.backend_ready ? 'bereit' : 'nicht verfuegbar' }}</span>
    </div>

    <div v-if="loading" class="history-state">Lade Verlauf aus dem Backend…</div>
    <div v-else-if="error" class="history-state error">{{ error }}</div>

    <div v-else class="history-grid">
      <section class="history-section">
        <div class="history-section-head">
          <h3>Sessions</h3>
          <span>{{ filteredSessions.length }}</span>
        </div>
        <div v-if="filteredSessions.length === 0" class="history-empty">
          Keine Sessions fuer die gewaehlten Filter.
        </div>
        <article v-for="session in filteredSessions" :key="session.id" class="session-card">
          <div class="session-row">
            <strong>{{ fmtDateTime(session.started_at_ms) }}</strong>
            <span class="chip">{{ session.end_reason || 'laufend' }}</span>
          </div>
          <div class="session-grid">
            <span>Dauer: {{ fmtDuration(session.duration_ms) }}</span>
            <span>Strecke: {{ fmtDist(session.distance_m) }}</span>
            <span>Akku: {{ fmtBattery(session.battery_start_v, session.battery_end_v) }}</span>
            <span v-if="session.mean_gps_accuracy_m != null">
              GPS Ø: {{ session.mean_gps_accuracy_m.toFixed(2) }} m
            </span>
          </div>
        </article>
      </section>

      <section class="history-section">
        <div class="history-section-head">
          <h3>Ereignisse</h3>
          <span>{{ filteredEvents.length }}</span>
        </div>
        <div v-if="filteredEvents.length === 0" class="history-empty">
          Keine Ereignisse fuer die gewaehlten Filter.
        </div>
        <article v-for="event in filteredEvents" :key="`${event.ts_ms}-${event.event_type}-${event.message}`" class="event-card">
          <div class="event-row">
            <strong>{{ fmtDateTime(event.ts_ms) }}</strong>
            <div class="event-chips">
              <span class="chip">{{ event.event_type }}</span>
              <span class="chip muted">{{ event.state_phase || 'unknown' }}</span>
              <span v-if="event.error_code" class="chip error">{{ event.error_code }}</span>
            </div>
          </div>
          <p class="event-message">{{ event.message }}</p>
          <div class="event-meta">
            <span>Grund: {{ event.event_reason || 'none' }}</span>
            <span v-if="event.battery_v != null">Akku: {{ event.battery_v.toFixed(1) }} V</span>
            <span v-if="event.gps_sol != null">GPS: {{ event.gps_sol }}</span>
          </div>
        </article>
      </section>
    </div>
  </div>
</template>

<style scoped>
.history-wrap {
  padding: 16px;
  min-height: 100%;
  background: #0a0f1a !important;
  color: #e2e8f0;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.history-header,
.history-toolbar,
.history-meta,
.history-section-head,
.session-row,
.event-row,
.event-meta {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.history-header {
  justify-content: space-between;
}

.history-title {
  margin: 0;
  font-size: 16px;
}

.history-sub,
.history-meta,
.history-empty,
.event-meta {
  color: #8aa0ba;
  font-size: 12px;
}

.history-actions {
  display: flex;
  gap: 8px;
  align-items: center;
  flex-wrap: wrap;
}

.history-btn {
  border: 1px solid #2563eb;
  background: #2563eb !important;
  color: white !important;
  border-radius: 8px;
  padding: 8px 12px;
  cursor: pointer;
  text-decoration: none;
}

.history-btn.secondary {
  background: transparent !important;
  color: #93c5fd !important;
  border-color: #1e3a5f;
}

.history-badge,
.chip {
  display: inline-flex;
  align-items: center;
  padding: 4px 8px;
  border-radius: 999px;
  font-size: 11px;
  border: 1px solid #1e3a5f;
  background: #0f1829 !important;
}

.chip.muted {
  color: #8aa0ba !important;
}

.chip.error {
  border-color: #7f1d1d;
  color: #fecaca !important;
  background: #2a0d0d !important;
}

.history-toolbar label {
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: 12px;
  color: #8aa0ba;
}

.history-toolbar select {
  min-width: 150px;
  background: #0f1829;
  color: #e2e8f0;
  border: 1px solid #1e3a5f;
  border-radius: 8px;
  padding: 8px 10px;
}

.history-check {
  flex-direction: row !important;
  align-items: center !important;
  margin-top: 18px;
}

.history-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 14px;
}

.history-section {
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
  border-radius: 14px;
  padding: 14px;
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.session-card,
.event-card {
  border: 1px solid #19314f;
  background: #09111d !important;
  border-radius: 12px;
  padding: 12px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.session-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px 12px;
  font-size: 12px;
  color: #d7e3f4;
}

.event-message {
  margin: 0;
  line-height: 1.45;
  font-size: 13px;
}

.event-chips {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
  margin-left: auto;
}

.history-state {
  border-radius: 12px;
  padding: 14px;
  background: #0f1829 !important;
  border: 1px solid #1e3a5f;
}

.history-state.error {
  border-color: #7f1d1d;
  color: #fecaca;
  background: #2a0d0d !important;
}

@media (max-width: 720px) {
  .session-grid {
    grid-template-columns: 1fr;
  }
}
</style>
