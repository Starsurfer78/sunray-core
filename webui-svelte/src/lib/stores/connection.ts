import { derived, writable } from 'svelte/store'

export type ReconnectState = 'idle' | 'connecting' | 'connected' | 'reconnecting' | 'error'

export interface ConnectionState {
  connected: boolean
  lastSeen: number
  reconnectState: ReconnectState
  attempts: number
}

const initialState: ConnectionState = {
  connected: false,
  lastSeen: 0,
  reconnectState: 'idle',
  attempts: 0,
}

function createConnectionStore() {
  const { subscribe, set, update } = writable<ConnectionState>(initialState)

  return {
    subscribe,
    reset: () => set(initialState),
    setConnecting: () => update((state) => ({
      ...state,
      reconnectState: state.attempts > 0 ? 'reconnecting' : 'connecting',
    })),
    setConnected: () => update((state) => ({
      ...state,
      connected: true,
      reconnectState: 'connected',
      lastSeen: Date.now(),
      attempts: 0,
    })),
    setDisconnected: () => update((state) => ({
      ...state,
      connected: false,
      reconnectState: 'error',
      attempts: state.attempts + 1,
    })),
    markSeen: () => update((state) => ({
      ...state,
      lastSeen: Date.now(),
    })),
  }
}

export const connection = createConnectionStore()

export const connectionLabel = derived(connection, ($connection) => {
  if ($connection.connected) return 'Verbunden'
  if ($connection.reconnectState === 'reconnecting') return 'Verbinde neu'
  if ($connection.reconnectState === 'connecting') return 'Verbinde'
  return 'Getrennt'
})
