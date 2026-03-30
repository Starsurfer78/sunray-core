import { writable } from 'svelte/store'

export interface Point {
  x: number
  y: number
}

export interface ZoneSettings {
  name: string
  stripWidth: number
  speed: number
  pattern: 'stripe' | 'spiral'
}

export interface Zone {
  id: string
  order: number
  polygon: Point[]
  settings: ZoneSettings
}

export type MapTool = 'perimeter' | 'dock' | 'zone' | 'nogo'

export interface RobotMap {
  perimeter: Point[]
  dock: Point[]
  mow: Point[]
  exclusions: Point[][]
  zones: Zone[]
}

export interface MapState {
  map: RobotMap
  selectedTool: MapTool
  selectedZoneId: string | null
  selectedExclusionIndex: number | null
  dirty: boolean
}

const defaultZoneSettings: ZoneSettings = {
  name: 'Zone',
  stripWidth: 0.18,
  speed: 1.0,
  pattern: 'stripe',
}

const emptyMap: RobotMap = {
  perimeter: [],
  dock: [],
  mow: [],
  exclusions: [],
  zones: [],
}

const initialState: MapState = {
  map: structuredClone(emptyMap),
  selectedTool: 'perimeter',
  selectedZoneId: null,
  selectedExclusionIndex: null,
  dirty: false,
}

function createId(prefix: string): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return `${prefix}-${crypto.randomUUID()}`
  }
  return `${prefix}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
}

function createZone(id: string, order: number): Zone {
  return {
    id,
    order,
    polygon: [],
    settings: {
      ...defaultZoneSettings,
      name: `Zone ${order}`,
    },
  }
}

function createMapStore() {
  const { subscribe, set, update } = writable<MapState>(initialState)

  return {
    subscribe,
    reset: () => set(initialState),
    load: (map: Partial<RobotMap>) => update((state) => ({
      ...state,
      map: {
        perimeter: map.perimeter ?? [],
        dock: map.dock ?? [],
        mow: map.mow ?? [],
        exclusions: map.exclusions ?? [],
        zones: map.zones ?? [],
      },
      selectedExclusionIndex: null,
      dirty: false,
    })),
    setTool: (selectedTool: MapTool) => update((state) => ({ ...state, selectedTool })),
    selectZone: (selectedZoneId: string | null) => update((state) => ({ ...state, selectedZoneId })),
    selectExclusion: (selectedExclusionIndex: number | null) => update((state) => ({
      ...state,
      selectedExclusionIndex,
    })),
    addPoint: (point: Point) => update((state) => {
      const next = structuredClone(state)
      if (next.selectedTool === 'perimeter') {
        next.map.perimeter.push(point)
      } else if (next.selectedTool === 'dock') {
        next.map.dock = [point]
      } else if (next.selectedTool === 'nogo') {
        if (next.selectedExclusionIndex === null) {
          next.map.exclusions.push([])
          next.selectedExclusionIndex = next.map.exclusions.length - 1
        }
        next.map.exclusions[next.selectedExclusionIndex]?.push(point)
      } else if (next.selectedTool === 'zone') {
        let zone = next.map.zones.find((entry) => entry.id === next.selectedZoneId)
        if (!zone) {
          const id = createId('zone')
          zone = createZone(id, next.map.zones.length + 1)
          next.map.zones.push(zone)
          next.selectedZoneId = id
        }
        zone.polygon.push(point)
      }
      next.dirty = true
      return next
    }),
    movePerimeterPoint: (index: number, point: Point) => update((state) => {
      const next = structuredClone(state)
      if (next.map.perimeter[index]) {
        next.map.perimeter[index] = point
        next.dirty = true
      }
      return next
    }),
    moveDockPoint: (index: number, point: Point) => update((state) => {
      const next = structuredClone(state)
      if (next.map.dock[index]) {
        next.map.dock[index] = point
        next.dirty = true
      }
      return next
    }),
    moveZonePoint: (zoneId: string, index: number, point: Point) => update((state) => {
      const next = structuredClone(state)
      const zone = next.map.zones.find((entry) => entry.id === zoneId)
      if (zone?.polygon[index]) {
        zone.polygon[index] = point
        next.dirty = true
      }
      return next
    }),
    moveExclusionPoint: (exclusionIndex: number, pointIndex: number, point: Point) => update((state) => {
      const next = structuredClone(state)
      if (next.map.exclusions[exclusionIndex]?.[pointIndex]) {
        next.map.exclusions[exclusionIndex][pointIndex] = point
        next.dirty = true
      }
      return next
    }),
    removeLastPoint: () => update((state) => {
      const next = structuredClone(state)
      if (next.selectedTool === 'perimeter') {
        next.map.perimeter.pop()
      } else if (next.selectedTool === 'dock') {
        next.map.dock.pop()
      } else if (next.selectedTool === 'nogo') {
        if (next.selectedExclusionIndex !== null) {
          next.map.exclusions[next.selectedExclusionIndex]?.pop()
        }
      } else if (next.selectedTool === 'zone') {
        const zone = next.map.zones.find((entry) => entry.id === next.selectedZoneId)
        zone?.polygon.pop()
      }
      next.dirty = true
      return next
    }),
    clearActive: () => update((state) => {
      const next = structuredClone(state)
      if (next.selectedTool === 'perimeter') {
        next.map.perimeter = []
      } else if (next.selectedTool === 'dock') {
        next.map.dock = []
      } else if (next.selectedTool === 'nogo') {
        if (next.selectedExclusionIndex !== null) {
          next.map.exclusions.splice(next.selectedExclusionIndex, 1)
          next.selectedExclusionIndex = next.map.exclusions.length > 0 ? 0 : null
        }
      } else if (next.selectedTool === 'zone') {
        if (next.selectedZoneId) {
          next.map.zones = next.map.zones.filter((entry) => entry.id !== next.selectedZoneId)
          next.selectedZoneId = next.map.zones[0]?.id ?? null
        }
      }
      next.dirty = true
      return next
    }),
    createExclusion: () => update((state) => {
      const next = structuredClone(state)
      next.map.exclusions.push([])
      next.selectedExclusionIndex = next.map.exclusions.length - 1
      next.selectedTool = 'nogo'
      next.dirty = true
      return next
    }),
    createZone: () => update((state) => {
      const next = structuredClone(state)
      const zone = createZone(createId('zone'), next.map.zones.length + 1)
      next.map.zones.push(zone)
      next.selectedZoneId = zone.id
      next.selectedTool = 'zone'
      next.dirty = true
      return next
    }),
    renameZone: (zoneId: string, name: string) => update((state) => {
      const next = structuredClone(state)
      const zone = next.map.zones.find((entry) => entry.id === zoneId)
      if (zone) zone.settings.name = name
      next.dirty = true
      return next
    }),
    markSaved: () => update((state) => ({ ...state, dirty: false })),
  }
}

export const mapStore = createMapStore()
