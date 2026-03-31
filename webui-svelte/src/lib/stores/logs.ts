import { writable } from 'svelte/store'

export interface LogEntry {
  id: number
  text: string
  ts: number
}

const MAX_ENTRIES = 200

function createLogStore() {
  const { subscribe, update } = writable<LogEntry[]>([])
  let seq = 0

  return {
    subscribe,
    push(text: string) {
      update((entries) => {
        const next = [...entries, { id: seq++, text, ts: Date.now() }]
        return next.length > MAX_ENTRIES ? next.slice(next.length - MAX_ENTRIES) : next
      })
    },
    clear() {
      update(() => [])
    },
  }
}

export const logStore = createLogStore()
