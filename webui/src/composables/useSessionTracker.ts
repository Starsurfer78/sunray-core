import { ref, watch, readonly, type Ref } from 'vue'
import { _telemetry } from './useTelemetry'

// ── Types ──────────────────────────────────────────────────────────────────────

export interface MowSession {
  id:         string              // ISO start timestamp (unique key)
  startedAt:  number              // Date.now() ms
  endedAt:    number              // Date.now() ms
  durationMs: number
  distanceM:  number              // integrated odometry (m)
  battStart:  number              // battery_v at start
  battEnd:    number              // battery_v at end
  path:       [number, number][]  // sampled [x, y] local metres
}

// ── Constants ─────────────────────────────────────────────────────────────────

const STORAGE_KEY          = 'sunray_sessions'
const MAX_SESSIONS         = 50
const PATH_SAMPLE_MS       = 2_000   // record one path point every 2 s
const MAX_PATH_POINTS      = 400
const MIN_SESSION_DURATION = 30_000  // discard sessions shorter than 30 s

// ── Persist helpers ───────────────────────────────────────────────────────────

function loadSessions(): MowSession[] {
  try { return JSON.parse(localStorage.getItem(STORAGE_KEY) ?? '[]') }
  catch { return [] }
}

function saveSessions(list: MowSession[]) {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(list)) }
  catch { /* quota exceeded — ignore */ }
}

// ── Singleton state ───────────────────────────────────────────────────────────

const sessions  = ref<MowSession[]>(loadSessions())
const recording = ref(false)

// Mutable working copy of the in-progress session (not reactive for perf)
let cur: {
  id:        string
  startedAt: number
  battStart: number
  distanceM: number
  path:      [number, number][]
} | null = null

let lastPathTs = 0
let lastX      = 0
let lastY      = 0

// ── Core watch (module-level, lives for the lifetime of the app) ──────────────

watch(_telemetry, (t) => {
  const mowing = (t.op === 'Mow')

  // ── Session start ───────────────────────────────────────────────────────────
  if (mowing && !recording.value) {
    recording.value = true
    lastX = t.x; lastY = t.y
    lastPathTs = Date.now()
    cur = {
      id:        new Date().toISOString(),
      startedAt: Date.now(),
      battStart: t.battery_v,
      distanceM: 0,
      path:      [[t.x, t.y]],
    }
    return
  }

  // ── Session end ─────────────────────────────────────────────────────────────
  if (!mowing && recording.value) {
    recording.value = false
    if (cur) {
      const now = Date.now()
      const duration = now - cur.startedAt
      if (duration >= MIN_SESSION_DURATION) {
        const session: MowSession = {
          id:         cur.id,
          startedAt:  cur.startedAt,
          endedAt:    now,
          durationMs: duration,
          distanceM:  cur.distanceM,
          battStart:  cur.battStart,
          battEnd:    t.battery_v,
          path:       cur.path,
        }
        sessions.value = [session, ...sessions.value].slice(0, MAX_SESSIONS)
        saveSessions(sessions.value)
      }
    }
    cur = null
    return
  }

  // ── In-session accumulation ─────────────────────────────────────────────────
  if (mowing && cur) {
    const dx = t.x - lastX
    const dy = t.y - lastY
    const d  = Math.sqrt(dx * dx + dy * dy)
    if (d > 0.02) {
      cur.distanceM += d
      lastX = t.x
      lastY = t.y
    }
    const now = Date.now()
    if (now - lastPathTs > PATH_SAMPLE_MS && cur.path.length < MAX_PATH_POINTS) {
      cur.path.push([t.x, t.y])
      lastPathTs = now
    }
  }
}, { deep: false })

// ── Public composable ─────────────────────────────────────────────────────────

export function useSessionTracker() {
  return {
    sessions:  readonly(sessions) as Ref<readonly MowSession[]>,
    recording: readonly(recording),

    clearSessions() {
      sessions.value = []
      saveSessions([])
    },

    deleteSession(id: string) {
      sessions.value = sessions.value.filter(s => s.id !== id)
      saveSessions(sessions.value)
    },
  }
}
