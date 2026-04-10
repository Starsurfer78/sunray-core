import { derived, writable } from 'svelte/store'
import { defaultTelemetry, type Telemetry } from '../api/types'

// Battery voltage range for percent calculation (LiPo 4S with 6 cells)
// Min: 22V (3.67V per cell), Max: 29.4V (4.9V per cell)
export const BATTERY_MIN_V = 22
export const BATTERY_MAX_V = 29.4

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
  const raw = (($telemetry.battery_v - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V)) * 100
  return Math.max(0, Math.min(100, Math.round(raw)))
})

export const gpsQuality = derived(telemetry, ($telemetry) => {
  if ($telemetry.gps_sol === 4) return 'RTK Fix'
  if ($telemetry.gps_sol === 5) return 'RTK Float'
  if ($telemetry.gps_sol > 0) return $telemetry.gps_text || 'GPS'
  return 'Kein Fix'
})
