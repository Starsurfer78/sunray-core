import { writable } from 'svelte/store'

export interface Point {
  x: number
  y: number
}

export type ExclusionType = 'hard' | 'soft'
export type DockApproachMode = 'forward_only' | 'reverse_allowed'

export interface PlannerSettings {
  defaultClearance: number
  perimeterSoftMargin: number
  perimeterHardMargin: number
  obstacleInflation: number
  softNoGoCostScale: number
  replanPeriodMs: number
  gridCellSize: number
}

export interface DockMeta {
  approachMode: DockApproachMode
  corridor: Point[]
  finalAlignHeadingDeg: number | null
  slowZoneRadius: number
}

export interface ExclusionMeta {
  type: ExclusionType
  clearance?: number
  costScale?: number
}

export interface Zone {
  id: string
  name: string
  order: number
  polygon: Point[]
}

export type MapTool = 'perimeter' | 'dock' | 'dock-corridor' | 'zone' | 'nogo' | 'move'

export interface RobotMap {
  perimeter: Point[]
  dock: Point[]
  exclusions: Point[][]
  exclusionMeta?: ExclusionMeta[]
  zones: Zone[]
  planner?: PlannerSettings
  dockMeta?: DockMeta
  captureMeta?: Record<string, unknown>
}

export interface MapState {
  map: RobotMap
  selectedTool: MapTool
  selectedZoneId: string | null
  selectedExclusionIndex: number | null
  dirty: boolean
}

export interface AddPointResult {
  accepted: boolean
  reason?: string
}

export type MapLoadDocument = Omit<Partial<RobotMap>, 'planner' | 'dockMeta'> & {
  planner?: Partial<PlannerSettings>
  dockMeta?: Partial<DockMeta>
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, value))
}

type Segment = { a: Point; b: Point }

const defaultPlannerSettings: PlannerSettings = {
  defaultClearance: 0.25,
  perimeterSoftMargin: 0.15,
  perimeterHardMargin: 0.05,
  obstacleInflation: 0.35,
  softNoGoCostScale: 0.6,
  replanPeriodMs: 250,
  gridCellSize: 0.1,
}

const defaultDockMeta: DockMeta = {
  approachMode: 'forward_only',
  corridor: [],
  finalAlignHeadingDeg: null,
  slowZoneRadius: 0.6,
}

const emptyMap: RobotMap = {
  perimeter: [],
  dock: [],
  exclusions: [],
  exclusionMeta: [],
  zones: [],
  planner: structuredClone(defaultPlannerSettings),
  dockMeta: structuredClone(defaultDockMeta),
  captureMeta: {},
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
    name: `Zone ${order}`,
    order,
    polygon: [],
  }
}

function normalizeZones(zones: Zone[] | undefined): Zone[] {
  if (!zones) return []
  return zones.map((zone, index) => ({
    id: zone.id,
    name: zone.name ?? `Zone ${index + 1}`,
    order: zone.order ?? index + 1, // legacy editor ordering until missions own sequencing
    polygon: zone.polygon ?? [],
  }))
}

function normalizePlannerSettings(settings: Partial<PlannerSettings> | undefined): PlannerSettings {
  return {
    defaultClearance: settings?.defaultClearance ?? defaultPlannerSettings.defaultClearance,
    perimeterSoftMargin: settings?.perimeterSoftMargin ?? defaultPlannerSettings.perimeterSoftMargin,
    perimeterHardMargin: settings?.perimeterHardMargin ?? defaultPlannerSettings.perimeterHardMargin,
    obstacleInflation: settings?.obstacleInflation ?? defaultPlannerSettings.obstacleInflation,
    softNoGoCostScale: settings?.softNoGoCostScale ?? defaultPlannerSettings.softNoGoCostScale,
    replanPeriodMs: settings?.replanPeriodMs ?? defaultPlannerSettings.replanPeriodMs,
    gridCellSize: settings?.gridCellSize ?? defaultPlannerSettings.gridCellSize,
  }
}

function normalizeDockMeta(meta: Partial<DockMeta> | undefined): DockMeta {
  return {
    approachMode: meta?.approachMode ?? defaultDockMeta.approachMode,
    corridor: meta?.corridor ?? [],
    finalAlignHeadingDeg: meta?.finalAlignHeadingDeg ?? defaultDockMeta.finalAlignHeadingDeg,
    slowZoneRadius: meta?.slowZoneRadius ?? defaultDockMeta.slowZoneRadius,
  }
}

function normalizeExclusionMeta(meta: ExclusionMeta[] | undefined, exclusionCount: number): ExclusionMeta[] {
  const normalized: ExclusionMeta[] = (meta ?? []).map((entry) => ({
    type: entry.type ?? 'hard',
    clearance: entry.clearance,
    costScale: entry.costScale,
  }))

  while (normalized.length < exclusionCount) {
    normalized.push({ type: 'hard' })
  }

  return normalized
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

function wouldCreateOpenPathIntersection(points: Point[], point: Point) {
  if (points.length < 3) return false

  const newSegment: Segment = {
    a: points[points.length - 1],
    b: point,
  }

  for (let i = 0; i < points.length - 2; i += 1) {
    const existing: Segment = {
      a: points[i],
      b: points[i + 1],
    }
    if (segmentsIntersect(existing, newSegment)) return true
  }

  return false
}

function isNearPoint(a: Point, b: Point, threshold = 0.03) {
  return Math.hypot(a.x - b.x, a.y - b.y) <= threshold
}

function wouldDuplicatePoint(points: Point[], point: Point) {
  return points.some((existing) => isNearPoint(existing, point))
}

function createMapStore() {
  const { subscribe, set, update } = writable<MapState>(initialState)

  return {
    subscribe,
    reset: () => set(initialState),
    load: (map: MapLoadDocument) => update((state) => ({
      ...state,
      map: {
        perimeter: map.perimeter ?? [],
        dock: map.dock ?? [],
        exclusions: map.exclusions ?? [],
        exclusionMeta: normalizeExclusionMeta(map.exclusionMeta, map.exclusions?.length ?? 0),
        zones: normalizeZones(map.zones),
        planner: normalizePlannerSettings(map.planner),
        dockMeta: normalizeDockMeta(map.dockMeta),
        captureMeta: map.captureMeta ?? {},
      },
      selectedExclusionIndex: null,
      dirty: false,
    })),
    restore: (map: MapLoadDocument) => update((state) => ({
      ...state,
      map: {
        perimeter: map.perimeter ?? [],
        dock: map.dock ?? [],
        exclusions: map.exclusions ?? [],
        exclusionMeta: normalizeExclusionMeta(map.exclusionMeta, map.exclusions?.length ?? 0),
        zones: normalizeZones(map.zones),
        planner: normalizePlannerSettings(map.planner),
        dockMeta: normalizeDockMeta(map.dockMeta),
        captureMeta: map.captureMeta ?? {},
      },
      selectedExclusionIndex: null,
      dirty: true,
    })),
    setTool: (selectedTool: MapTool) => update((state) => ({ ...state, selectedTool })),
    selectZone: (selectedZoneId: string | null) => update((state) => ({ ...state, selectedZoneId })),
    selectExclusion: (selectedExclusionIndex: number | null) => update((state) => ({
      ...state,
      selectedExclusionIndex,
    })),
    addPoint: (point: Point): AddPointResult => {
      let result: AddPointResult = { accepted: false }

      update((state) => {
        const next = structuredClone(state)
        if (next.selectedTool === 'perimeter') {
          if (wouldDuplicatePoint(next.map.perimeter, point)) {
            result = { accepted: false, reason: 'Punkt liegt bereits an dieser Stelle' }
          } else if (!wouldCreateOpenPathIntersection(next.map.perimeter, point)) {
            next.map.perimeter.push(point)
            result = { accepted: true }
          } else {
            result = { accepted: false, reason: 'Punkt wuerde Polygon schneiden' }
          }
        } else if (next.selectedTool === 'dock') {
          if (wouldDuplicatePoint(next.map.dock, point)) {
            result = { accepted: false, reason: 'Punkt liegt bereits an dieser Stelle' }
          } else {
            next.map.dock.push(point)
            result = { accepted: true }
          }
        } else if (next.selectedTool === 'dock-corridor') {
          next.map.dockMeta = normalizeDockMeta(next.map.dockMeta)
          if (wouldDuplicatePoint(next.map.dockMeta.corridor, point)) {
            result = { accepted: false, reason: 'Punkt liegt bereits an dieser Stelle' }
          } else if (!wouldCreateOpenPathIntersection(next.map.dockMeta.corridor, point)) {
            next.map.dockMeta.corridor.push(point)
            result = { accepted: true }
          } else {
            result = { accepted: false, reason: 'Punkt wuerde Polygon schneiden' }
          }
        } else if (next.selectedTool === 'nogo') {
          if (next.selectedExclusionIndex === null) {
            next.map.exclusions.push([])
            next.map.exclusionMeta ??= []
            next.map.exclusionMeta.push({ type: 'hard' })
            next.selectedExclusionIndex = next.map.exclusions.length - 1
          }
          const exclusion = next.map.exclusions[next.selectedExclusionIndex]
          if (exclusion && wouldDuplicatePoint(exclusion, point)) {
            result = { accepted: false, reason: 'Punkt liegt bereits an dieser Stelle' }
          } else if (exclusion && !wouldCreateOpenPathIntersection(exclusion, point)) {
            exclusion.push(point)
            result = { accepted: true }
          } else {
            result = { accepted: false, reason: 'Punkt wuerde Polygon schneiden' }
          }
        } else if (next.selectedTool === 'zone') {
          let zone = next.map.zones.find((entry) => entry.id === next.selectedZoneId)
          if (!zone) {
            const id = createId('zone')
            zone = createZone(id, next.map.zones.length + 1)
            next.map.zones.push(zone)
            next.selectedZoneId = id
          }
          if (wouldDuplicatePoint(zone.polygon, point)) {
            result = { accepted: false, reason: 'Punkt liegt bereits an dieser Stelle' }
          } else if (!wouldCreateOpenPathIntersection(zone.polygon, point)) {
            zone.polygon.push(point)
            result = { accepted: true }
          } else {
            result = { accepted: false, reason: 'Punkt wuerde Polygon schneiden' }
          }
        }
        if (result.accepted) {
          next.dirty = true
        }
        return next
      })

      return result
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
    moveDockCorridorPoint: (index: number, point: Point) => update((state) => {
      const next = structuredClone(state)
      next.map.dockMeta = normalizeDockMeta(next.map.dockMeta)
      if (next.map.dockMeta.corridor[index]) {
        next.map.dockMeta.corridor[index] = point
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
    deleteDockCorridorPoint: (index: number) => update((state) => {
      const next = structuredClone(state)
      next.map.dockMeta = normalizeDockMeta(next.map.dockMeta)
      if (index >= 0 && index < next.map.dockMeta.corridor.length) {
        next.map.dockMeta.corridor.splice(index, 1)
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
        next.map.exclusionMeta?.splice(next.selectedExclusionIndex, 1)
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
      } else if (next.selectedTool === 'dock-corridor') {
        next.map.dockMeta = normalizeDockMeta(next.map.dockMeta)
        next.map.dockMeta.corridor.pop()
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
      } else if (next.selectedTool === 'dock-corridor') {
        next.map.dockMeta = normalizeDockMeta(next.map.dockMeta)
        next.map.dockMeta.corridor = []
      } else if (next.selectedTool === 'nogo') {
        if (next.selectedExclusionIndex !== null) {
          next.map.exclusions.splice(next.selectedExclusionIndex, 1)
          next.map.exclusionMeta?.splice(next.selectedExclusionIndex, 1)
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
      next.map.exclusionMeta ??= []
      next.map.exclusionMeta.push({ type: 'hard' })
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
      if (zone) zone.name = name
      next.dirty = true
      return next
    }),
    updatePlannerSettings: (patch: Partial<PlannerSettings>) => update((state) => {
      const next = structuredClone(state)
      next.map.planner = normalizePlannerSettings({
        ...next.map.planner,
        ...patch,
      })
      next.map.planner.defaultClearance = clamp(next.map.planner.defaultClearance, 0.05, 2)
      next.map.planner.perimeterSoftMargin = clamp(next.map.planner.perimeterSoftMargin, 0, 2)
      next.map.planner.perimeterHardMargin = clamp(next.map.planner.perimeterHardMargin, 0, 2)
      next.map.planner.obstacleInflation = clamp(next.map.planner.obstacleInflation, 0.05, 2)
      next.map.planner.softNoGoCostScale = clamp(next.map.planner.softNoGoCostScale, 0, 4)
      next.map.planner.replanPeriodMs = Math.round(clamp(next.map.planner.replanPeriodMs, 50, 5000))
      next.map.planner.gridCellSize = clamp(next.map.planner.gridCellSize, 0.05, 0.5)
      next.dirty = true
      return next
    }),
    updateDockMeta: (patch: Partial<DockMeta>) => update((state) => {
      const next = structuredClone(state)
      next.map.dockMeta = normalizeDockMeta({
        ...next.map.dockMeta,
        ...patch,
      })
      next.map.dockMeta.slowZoneRadius = clamp(next.map.dockMeta.slowZoneRadius, 0.1, 3)
      if (next.map.dockMeta.finalAlignHeadingDeg !== null) {
        let heading = next.map.dockMeta.finalAlignHeadingDeg % 360
        if (heading < 0) heading += 360
        next.map.dockMeta.finalAlignHeadingDeg = heading
      }
      next.dirty = true
      return next
    }),
    updateExclusionMeta: (exclusionIndex: number, patch: Partial<ExclusionMeta>) => update((state) => {
      const next = structuredClone(state)
      next.map.exclusionMeta = normalizeExclusionMeta(next.map.exclusionMeta, next.map.exclusions.length)
      const current = next.map.exclusionMeta[exclusionIndex]
      if (!current) return next
      next.map.exclusionMeta[exclusionIndex] = {
        ...current,
        ...patch,
      }
      if (next.map.exclusionMeta[exclusionIndex].clearance !== undefined) {
        next.map.exclusionMeta[exclusionIndex].clearance = clamp(
          next.map.exclusionMeta[exclusionIndex].clearance ?? 0.25,
          0,
          2,
        )
      }
      if (next.map.exclusionMeta[exclusionIndex].costScale !== undefined) {
        next.map.exclusionMeta[exclusionIndex].costScale = clamp(
          next.map.exclusionMeta[exclusionIndex].costScale ?? 1,
          0,
          4,
        )
      }
      next.dirty = true
      return next
    }),
    markSaved: () => update((state) => ({ ...state, dirty: false })),
  }
}

export const mapStore = createMapStore()
