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

export type MapTool = 'perimeter' | 'dock' | 'zone' | 'nogo' | 'move'

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

type Segment = { a: Point; b: Point }

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

function orientation(a: Point, b: Point, c: Point) {
  return (b.y - a.y) * (c.x - b.x) - (b.x - a.x) * (c.y - b.y)
}

function onSegment(a: Point, b: Point, c: Point) {
  return (
    Math.min(a.x, c.x) <= b.x &&
    b.x <= Math.max(a.x, c.x) &&
    Math.min(a.y, c.y) <= b.y &&
    b.y <= Math.max(a.y, c.y)
  )
}

function segmentsIntersect(s1: Segment, s2: Segment) {
  const o1 = orientation(s1.a, s1.b, s2.a)
  const o2 = orientation(s1.a, s1.b, s2.b)
  const o3 = orientation(s2.a, s2.b, s1.a)
  const o4 = orientation(s2.a, s2.b, s1.b)

  if (o1 === 0 && onSegment(s1.a, s2.a, s1.b)) return true
  if (o2 === 0 && onSegment(s1.a, s2.b, s1.b)) return true
  if (o3 === 0 && onSegment(s2.a, s1.a, s2.b)) return true
  if (o4 === 0 && onSegment(s2.a, s1.b, s2.b)) return true

  return (o1 > 0) !== (o2 > 0) && (o3 > 0) !== (o4 > 0)
}

function hasSelfIntersection(points: Point[]) {
  if (points.length < 4) return false

  const segments: Segment[] = points.map((point, index) => ({
    a: point,
    b: points[(index + 1) % points.length],
  }))

  for (let i = 0; i < segments.length; i += 1) {
    for (let j = i + 1; j < segments.length; j += 1) {
      const adjacent = j === i + 1 || (i === 0 && j === segments.length - 1)
      if (adjacent) continue
      if (segmentsIntersect(segments[i], segments[j])) return true
    }
  }

  return false
}

function wouldCreateSelfIntersection(points: Point[], point: Point) {
  return hasSelfIntersection([...points, point])
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
    addPoint: (point: Point) => {
      let accepted = false

      update((state) => {
      const next = structuredClone(state)
      if (next.selectedTool === 'perimeter') {
        if (!wouldCreateSelfIntersection(next.map.perimeter, point)) {
          next.map.perimeter.push(point)
          accepted = true
        }
      } else if (next.selectedTool === 'dock') {
        next.map.dock = [point]
        accepted = true
      } else if (next.selectedTool === 'nogo') {
        if (next.selectedExclusionIndex === null) {
          next.map.exclusions.push([])
          next.selectedExclusionIndex = next.map.exclusions.length - 1
        }
        const exclusion = next.map.exclusions[next.selectedExclusionIndex]
        if (exclusion && !wouldCreateSelfIntersection(exclusion, point)) {
          exclusion.push(point)
          accepted = true
        }
      } else if (next.selectedTool === 'zone') {
        let zone = next.map.zones.find((entry) => entry.id === next.selectedZoneId)
        if (!zone) {
          const id = createId('zone')
          zone = createZone(id, next.map.zones.length + 1)
          next.map.zones.push(zone)
          next.selectedZoneId = id
        }
        if (!wouldCreateSelfIntersection(zone.polygon, point)) {
          zone.polygon.push(point)
          accepted = true
        }
      }
      if (accepted) {
        next.dirty = true
      }
      return next
      })

      return accepted
    },
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
    deletePerimeterPoint: (index: number) => update((state) => {
      const next = structuredClone(state)
      if (index >= 0 && index < next.map.perimeter.length) {
        next.map.perimeter.splice(index, 1)
        next.dirty = true
      }
      return next
    }),
    deleteDockPoint: (index: number) => update((state) => {
      const next = structuredClone(state)
      if (index >= 0 && index < next.map.dock.length) {
        next.map.dock.splice(index, 1)
        next.dirty = true
      }
      return next
    }),
    deleteZonePoint: (zoneId: string, index: number) => update((state) => {
      const next = structuredClone(state)
      const zone = next.map.zones.find((entry) => entry.id === zoneId)
      if (zone && index >= 0 && index < zone.polygon.length) {
        zone.polygon.splice(index, 1)
        next.dirty = true
      }
      return next
    }),
    deleteExclusionPoint: (exclusionIndex: number, pointIndex: number) => update((state) => {
      const next = structuredClone(state)
      if (exclusionIndex >= 0 && exclusionIndex < next.map.exclusions.length) {
        const exclusion = next.map.exclusions[exclusionIndex]
        if (pointIndex >= 0 && pointIndex < exclusion.length) {
          exclusion.splice(pointIndex, 1)
          next.dirty = true
        }
      }
      return next
    }),
    deleteSelectedZone: () => update((state) => {
      const next = structuredClone(state)
      if (next.selectedZoneId) {
        next.map.zones = next.map.zones.filter((entry) => entry.id !== next.selectedZoneId)
        next.selectedZoneId = next.map.zones[0]?.id ?? null
        next.dirty = true
      }
      return next
    }),
    deleteSelectedExclusion: () => update((state) => {
      const next = structuredClone(state)
      if (next.selectedExclusionIndex !== null) {
        next.map.exclusions.splice(next.selectedExclusionIndex, 1)
        next.selectedExclusionIndex = next.map.exclusions.length > 0 ? 0 : null
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
