import { ref, readonly, onUnmounted, type Ref } from 'vue'

// ── Telemetry types (mirrors WebSocketServer::TelemetryData) ─────────────────

export interface Telemetry {
  type:       string
  op:         string   // Idle | Mow | Dock | Charge | EscapeReverse | GpsWait | Error
  x:          number   // local metres east
  y:          number   // local metres north
  heading:    number   // radians, 0 = east
  battery_v:  number
  charge_v:   number
  gps_sol:    number   // 0=none 4=RTK-Fix 5=RTK-Float
  gps_text:   string
  gps_acc:    number   // horizontal accuracy estimate in metres
  gps_lat:    number
  gps_lon:    number
  bumper_l:   boolean
  bumper_r:   boolean
  lift:        boolean
  motor_err:   boolean
  uptime_s:    number
  diag_active: boolean  // true while a diag motor test is running
  diag_ticks:  number   // live accumulated ticks in current diag test
  mcu_v?:      string   // MCU firmware version
  pi_v?:       string   // Pi software version
  imu_h?:      number   // IMU heading [deg]
  imu_r?:      number   // IMU roll [deg]
  imu_p?:      number   // IMU pitch [deg]
  ekf_health?: string   // fusion mode: "EKF+GPS" | "EKF+IMU" | "Odo"
  state_phase?: string
  resume_target?: string
  event_reason?: string
  error_code?: string
}

const defaultTelemetry: Telemetry = {
  type: '', op: 'Idle', x: 0, y: 0, heading: 0,
  battery_v: 0, charge_v: 0, gps_sol: 0, gps_text: '---',
  gps_acc: 0, gps_lat: 0, gps_lon: 0, bumper_l: false, bumper_r: false, lift: false,
  motor_err: false, uptime_s: 0, diag_active: false, diag_ticks: 0,
  mcu_v: '', pi_v: '', imu_h: 0, imu_r: 0, imu_p: 0,
  state_phase: 'idle', resume_target: '', event_reason: 'none', error_code: ''
}

// ── Singleton state ───────────────────────────────────────────────────────────

const telemetry  = ref<Telemetry>({ ...defaultTelemetry })
const connected  = ref(false)

// Exported raw refs for cross-composable use (e.g. useSessionTracker).
// Do NOT use these directly in components — call useTelemetry() instead.
export const _telemetry = telemetry
export const _connected = connected
const lastUpdate = ref(0)
const logs       = ref<string[]>([])

const MAX_LOG_LINES = 200

let ws:          WebSocket | null = null
let reconnTimer: ReturnType<typeof setTimeout> | null = null
let refCount = 0

function connect() {
  if (ws && ws.readyState <= WebSocket.OPEN) return

  const proto = location.protocol === 'https:' ? 'wss' : 'ws'
  const url   = `${proto}://${location.host}/ws/telemetry`

  ws = new WebSocket(url)

  ws.onopen = () => {
    connected.value = true
    if (reconnTimer) { clearTimeout(reconnTimer); reconnTimer = null }
  }

  ws.onmessage = (ev) => {
    try {
      const msg = JSON.parse(ev.data)
      if (msg.type === 'state') {
        telemetry.value  = msg as Telemetry
        lastUpdate.value = Date.now()
      } else if (msg.type === 'log' && typeof msg.text === 'string') {
        const ts = new Date().toLocaleTimeString('de-DE', { hour12: false })
        logs.value.unshift(`[${ts}] ${msg.text}`)
        if (logs.value.length > MAX_LOG_LINES) logs.value.length = MAX_LOG_LINES
      }
    } catch { /* ignore malformed */ }
  }

  ws.onclose = ws.onerror = () => {
    connected.value = false
    ws = null
    if (refCount > 0) {
      reconnTimer = setTimeout(connect, 3000)
    }
  }
}

function disconnect() {
  if (reconnTimer) { clearTimeout(reconnTimer); reconnTimer = null }
  ws?.close()
  ws = null
  connected.value = false
}

// ── Command sender ────────────────────────────────────────────────────────────

function sendCmd(cmd: string, extra?: Record<string, unknown>) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return
  const token = localStorage.getItem('sunray_api_token') ?? ''
  ws.send(JSON.stringify({ cmd, ...(token ? { token } : {}), ...extra }))
}

// ── Public composable ─────────────────────────────────────────────────────────

export function useTelemetry() {
  refCount++
  connect()

  onUnmounted(() => {
    refCount--
    if (refCount <= 0) disconnect()
  })

  return {
    telemetry:  readonly(telemetry),
    connected:  readonly(connected),
    lastUpdate: readonly(lastUpdate),
    logs:       readonly(logs) as Ref<readonly string[]>,
    sendCmd,
    clearLogs:  () => { logs.value = [] },
  }
}
