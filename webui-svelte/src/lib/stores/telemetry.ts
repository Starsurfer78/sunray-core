import { derived, writable } from 'svelte/store'
import { defaultTelemetry, type Telemetry } from '../api/types'

function createTelemetryStore() {
  const { subscribe, set, update } = writable<Telemetry>({ ...defaultTelemetry })

  return {
    subscribe,
    reset: () => set({ ...defaultTelemetry }),
    set: (value: Telemetry) => set(value),
    patch: (value: Partial<Telemetry>) => update((state) => ({ ...state, ...value })),
  }
}

export const telemetry = createTelemetryStore()

export const batteryPercent = derived(telemetry, ($telemetry) => {
  const raw = (($telemetry.battery_v - 22) / (29.4 - 22)) * 100
  return Math.max(0, Math.min(100, Math.round(raw)))
})

export const gpsQuality = derived(telemetry, ($telemetry) => {
  if ($telemetry.gps_sol === 4) return 'RTK Fix'
  if ($telemetry.gps_sol === 5) return 'RTK Float'
  if ($telemetry.gps_sol > 0) return $telemetry.gps_text || 'GPS'
  return 'Kein Fix'
})
