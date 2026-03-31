import { computed, onMounted, readonly, ref, type Ref } from 'vue'

export interface HistoryEventItem {
  ts_ms: number
  level: string
  module: string
  event_type: string
  state_phase: string
  event_reason: string
  error_code?: string
  message: string
  battery_v?: number
  gps_sol?: number
  x?: number
  y?: number
}

export interface HistorySessionItem {
  id: string
  started_at_ms: number
  ended_at_ms?: number
  duration_ms: number
  distance_m: number
  battery_start_v: number
  battery_end_v: number
  end_reason: string
  mean_gps_accuracy_m?: number
  max_gps_accuracy_m?: number
  metadata?: Record<string, unknown>
}

export interface HistorySummary {
  ok: boolean
  backend_ready: boolean
  events_total: number
  sessions_total: number
  sessions_completed: number
  mowing_duration_ms_total: number
  mowing_distance_m_total: number
  last_event_ts_ms: number
  last_session_started_at_ms: number
  retention: {
    max_events: number
    max_sessions: number
  }
  export_enabled: boolean
  database_path: string
  database_exists: boolean
}

const events = ref<HistoryEventItem[]>([])
const sessions = ref<HistorySessionItem[]>([])
const summary = ref<HistorySummary | null>(null)
const loading = ref(false)
const error = ref('')
const lastUpdated = ref(0)
let refreshPromise: Promise<void> | null = null

async function fetchJson<T>(url: string): Promise<T> {
  const res = await fetch(url)
  if (!res.ok) {
    let message = `${res.status} ${res.statusText}`
    try {
      const body = await res.json()
      if (typeof body.error === 'string' && body.error) message = body.error
    } catch {
      // ignore parse failure
    }
    throw new Error(message)
  }
  return res.json() as Promise<T>
}

async function refreshHistoryData() {
  if (refreshPromise) return refreshPromise

  refreshPromise = (async () => {
    loading.value = true
    error.value = ''
    try {
      const [eventsRes, sessionsRes, summaryRes] = await Promise.all([
        fetchJson<{ items: HistoryEventItem[] }>('/api/history/events?limit=300'),
        fetchJson<{ items: HistorySessionItem[] }>('/api/history/sessions?limit=120'),
        fetchJson<HistorySummary>('/api/statistics/summary'),
      ])

      events.value = Array.isArray(eventsRes.items) ? eventsRes.items : []
      sessions.value = Array.isArray(sessionsRes.items) ? sessionsRes.items : []
      summary.value = summaryRes
      lastUpdated.value = Date.now()
    } catch (err) {
      error.value = err instanceof Error ? err.message : 'History backend nicht erreichbar'
    } finally {
      loading.value = false
      refreshPromise = null
    }
  })()

  return refreshPromise
}

const backendReady = computed(() => summary.value?.backend_ready ?? false)

export function useHistoryBackend() {
  onMounted(() => {
    void refreshHistoryData()
  })

  return {
    events: readonly(events) as Ref<readonly HistoryEventItem[]>,
    sessions: readonly(sessions) as Ref<readonly HistorySessionItem[]>,
    summary: readonly(summary) as Ref<HistorySummary | null>,
    loading: readonly(loading),
    error: readonly(error),
    lastUpdated: readonly(lastUpdated),
    backendReady,
    refreshHistoryData,
  }
}
