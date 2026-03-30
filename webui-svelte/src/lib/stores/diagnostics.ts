import { writable } from 'svelte/store'
import type { DiagResponse } from '../api/rest'

export interface DiagnosticsState {
  busy: boolean
  activeAction: string | null
  lastResult: DiagResponse | null
  error: string
}

const initialState: DiagnosticsState = {
  busy: false,
  activeAction: null,
  lastResult: null,
  error: '',
}

function createDiagnosticsStore() {
  const { subscribe, set, update } = writable<DiagnosticsState>(initialState)

  return {
    subscribe,
    reset: () => set(initialState),
    start: (activeAction: string) => update((state) => ({
      ...state,
      busy: true,
      activeAction,
      error: '',
    })),
    success: (lastResult: DiagResponse) => update(() => ({
      busy: false,
      activeAction: null,
      lastResult,
      error: '',
    })),
    fail: (error: string) => update((state) => ({
      ...state,
      busy: false,
      activeAction: null,
      error,
    })),
  }
}

export const diagnostics = createDiagnosticsStore()
