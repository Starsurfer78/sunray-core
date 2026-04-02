import { connection } from '../stores/connection'
import { telemetry } from '../stores/telemetry'
import type { TelemetryEnvelope, WsMessage } from './types'
import { logStore } from '../stores/logs'
import { commandFeedback } from '../stores/commandFeedback'

let ws: WebSocket | null = null
let reconnectTimer: ReturnType<typeof setTimeout> | null = null
let active = false

function clearReconnectTimer() {
  if (reconnectTimer) {
    clearTimeout(reconnectTimer)
    reconnectTimer = null
  }
}

function scheduleReconnect() {
  clearReconnectTimer()
  reconnectTimer = setTimeout(() => {
    if (active) connect()
  }, 3000)
}

function connect() {
  if (!active) return
  if (ws && ws.readyState <= WebSocket.OPEN) return

  connection.setConnecting()

  const proto = location.protocol === 'https:' ? 'wss' : 'ws'
  const url = `${proto}://${location.host}/ws/telemetry`
  ws = new WebSocket(url)

  ws.onopen = () => {
    connection.setConnected()
    clearReconnectTimer()
  }

  ws.onmessage = (event) => {
    connection.markSeen()
    try {
      const message = JSON.parse(event.data) as WsMessage
      if (message.type === 'state') {
        // The current backend sends full state packets, but patching keeps the
        // UI robust if a future backend revision sends partial deltas.
        telemetry.patch(message as Partial<TelemetryEnvelope>)
      } else if (message.type === 'log') {
        logStore.push(message.text)
      }
    } catch {
      // Ignore malformed websocket payloads in the MVP.
    }
  }

  const handleDisconnect = () => {
    if (ws === null) return
    ws = null
    connection.setDisconnected()
    if (active) scheduleReconnect()
  }

  ws.onclose = handleDisconnect
  ws.onerror = handleDisconnect
}

export function startTelemetry() {
  active = true
  connect()
}

export function stopTelemetry() {
  active = false
  clearReconnectTimer()
  if (ws) {
    ws.close()
    ws = null
  }
  connection.reset()
}

export function sendCmd(cmd: string, extra: Record<string, unknown> = {}) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    commandFeedback.show(`Befehl "${cmd}" wurde nicht gesendet. Keine aktive WebSocket-Verbindung.`)
    return false
  }

  try {
    ws.send(JSON.stringify({ cmd, ...extra }))
    return true
  } catch {
    commandFeedback.show(`Befehl "${cmd}" konnte nicht gesendet werden.`)
    return false
  }
}
