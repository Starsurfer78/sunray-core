import { writable } from 'svelte/store'
import type { Zone } from './map'
import { createMissionId } from '../utils/idGenerator'

export interface MissionSchedule {
  enabled: boolean
  days: number[]
  startTime: string
  endTime: string
  rainDelayMinutes: number
}

export interface MissionZoneOverrides {
  stripWidth?: number
  angle?: number
  edgeMowing?: boolean
  edgeRounds?: number
  speed?: number
  pattern?: 'stripe' | 'spiral'
}

export interface PlanRef {
  id: string
  revision?: number
  generatedAtMs?: number
}

export interface Mission {
  id: string
  name: string
  zoneIds: string[]
  overrides: Record<string, MissionZoneOverrides>
  schedule?: MissionSchedule
  planRef?: PlanRef
}

interface MissionState {
  missions: Mission[]
  selectedMissionId: string | null
}

const STORAGE_KEY = 'sunray_svelte_missions'

function defaultSchedule(): MissionSchedule {
  return {
    enabled: false,
    days: [],
    startTime: '09:00',
    endTime: '12:00',
    rainDelayMinutes: 60,
  }
}

function createMission(name: string, zoneIds: string[] = []): Mission {
  return {
    id: createMissionId(),
    name,
    zoneIds,
    overrides: {},
    schedule: defaultSchedule(),
  }
}

function normalizeMission(raw: Partial<Mission>, index: number): Mission {
  return {
    id: raw.id ?? createMissionId(),
    name: raw.name?.trim() || `Mission ${index + 1}`,
    zoneIds: Array.isArray(raw.zoneIds) ? raw.zoneIds.filter((entry): entry is string => typeof entry === 'string') : [],
    overrides: raw.overrides ?? {},
    planRef: raw.planRef && typeof raw.planRef.id === 'string'
      ? {
        id: raw.planRef.id,
        revision: typeof raw.planRef.revision === 'number' ? raw.planRef.revision : undefined,
        generatedAtMs: typeof raw.planRef.generatedAtMs === 'number' ? raw.planRef.generatedAtMs : undefined,
      }
      : undefined,
    schedule: {
      ...defaultSchedule(),
      ...(raw.schedule ?? {}),
      days: Array.isArray(raw.schedule?.days)
        ? raw.schedule.days.filter((entry): entry is number => Number.isInteger(entry) && entry >= 0 && entry <= 6)
        : [],
    },
  }
}

function loadStoredState(): MissionState {
  if (typeof localStorage === 'undefined') {
    return { missions: [], selectedMissionId: null }
  }

  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (!raw) return { missions: [], selectedMissionId: null }
    const parsed = JSON.parse(raw) as Partial<MissionState>
    const missions = Array.isArray(parsed.missions)
      ? parsed.missions.map((mission, index) => normalizeMission(mission, index))
      : []
    const selectedMissionId = missions.find((mission) => mission.id === parsed.selectedMissionId)?.id ?? missions[0]?.id ?? null
    return { missions, selectedMissionId }
  } catch {
    return { missions: [], selectedMissionId: null }
  }
}

function persistState(state: MissionState) {
  if (typeof localStorage === 'undefined') return
  localStorage.setItem(STORAGE_KEY, JSON.stringify(state))
}

function sortZoneIds(zoneIds: string[], zones: Zone[]): string[] {
  const zoneOrder = new Map(zones.map((zone) => [zone.id, zone.order]))
  return [...zoneIds].sort((left, right) => (zoneOrder.get(left) ?? 999) - (zoneOrder.get(right) ?? 999))
}

function createMissionStore() {
  const { subscribe, update, set } = writable<MissionState>(loadStoredState())

  const apply = (updater: (state: MissionState) => MissionState) =>
    update((state) => {
      const next = updater(state)
      persistState(next)
      return next
    })

  return {
    subscribe,
    replaceAll: (missions: Mission[], zones: Zone[] = []) => {
      const next = sanitizeMissionState({
        missions,
        selectedMissionId: missions[0]?.id ?? null,
      }, zones)
      persistState(next)
      set(next)
    },
    reset: () => {
      const next = { missions: [], selectedMissionId: null }
      persistState(next)
      set(next)
    },
    ensureSeedMission: (zones: Zone[]) => apply((state) => {
      if (state.missions.length > 0) return sanitizeMissionState(state, zones)
      const seed = createMission('Wochenmähung', sortZoneIds(zones.map((zone) => zone.id), zones))
      return { missions: [seed], selectedMissionId: seed.id }
    }),
    selectMission: (missionId: string | null) => apply((state) => ({
      ...state,
      selectedMissionId: state.missions.find((mission) => mission.id === missionId)?.id ?? state.missions[0]?.id ?? null,
    })),
    createMission: (zones: Zone[]) => {
      let createdMissionId = ''
      apply((state) => {
        const mission = createMission(`Mission ${state.missions.length + 1}`)
        createdMissionId = mission.id
        return {
          missions: [...state.missions, mission],
          selectedMissionId: mission.id,
        }
      })
      return createdMissionId
    },
    deleteMission: (missionId: string) => apply((state) => {
      const missions = state.missions.filter((mission) => mission.id !== missionId)
      return {
        missions,
        selectedMissionId: state.selectedMissionId === missionId ? missions[0]?.id ?? null : state.selectedMissionId,
      }
    }),
    renameMission: (missionId: string, name: string) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) =>
        mission.id === missionId ? { ...mission, name: name.trim() || mission.name } : mission,
      ),
    })),
    setMissionPlanRef: (missionId: string, planRef: PlanRef | undefined) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) =>
        mission.id === missionId ? { ...mission, planRef } : mission,
      ),
    })),
    setMissionZoneIds: (missionId: string, zoneIds: string[], zones: Zone[]) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) =>
        mission.id === missionId
          ? { ...mission, zoneIds: sortZoneIds(zoneIds, zones), planRef: undefined }
          : mission,
      ),
    })),
    toggleMissionDay: (missionId: string, day: number) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) => {
        if (mission.id !== missionId || !mission.schedule) return mission
        const days = mission.schedule.days.includes(day)
          ? mission.schedule.days.filter((entry) => entry !== day)
          : [...mission.schedule.days, day].sort((a, b) => a - b)
        return { ...mission, schedule: { ...mission.schedule, days } }
      }),
    })),
    updateMissionSchedule: (missionId: string, patch: Partial<MissionSchedule>) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) =>
        mission.id === missionId
          ? { ...mission, schedule: { ...defaultSchedule(), ...(mission.schedule ?? {}), ...patch } }
          : mission,
      ),
    })),
    updateMissionZoneOverride: (missionId: string, zoneId: string, patch: Partial<MissionZoneOverrides>) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) => {
        if (mission.id !== missionId) return mission
        return {
          ...mission,
          planRef: undefined,
          overrides: {
            ...mission.overrides,
            [zoneId]: {
              ...(mission.overrides[zoneId] ?? {}),
              ...patch,
            },
          },
        }
      }),
    })),
    clearMissionZoneOverride: (missionId: string, zoneId: string) => apply((state) => ({
      ...state,
      missions: state.missions.map((mission) => {
        if (mission.id !== missionId) return mission
        const overrides = { ...mission.overrides }
        delete overrides[zoneId]
        return { ...mission, overrides, planRef: undefined }
      }),
    })),
    syncZones: (zones: Zone[]) => apply((state) => sanitizeMissionState(state, zones)),
  }
}

function sanitizeMissionState(state: MissionState, zones: Zone[]): MissionState {
  const validZoneIds = new Set(zones.map((zone) => zone.id))
  const missions = state.missions.map((mission, index) => {
    const zoneIds = sortZoneIds(mission.zoneIds.filter((zoneId) => validZoneIds.has(zoneId)), zones)
    const overrides = Object.fromEntries(
      Object.entries(mission.overrides).filter(([zoneId]) => validZoneIds.has(zoneId)),
    )
    return {
      ...normalizeMission(mission, index),
      zoneIds,
      overrides,
      planRef:
        mission.planRef && zoneIds.length === mission.zoneIds.length
          ? mission.planRef
          : undefined,
    }
  })
  return {
    missions,
    selectedMissionId: missions.find((mission) => mission.id === state.selectedMissionId)?.id ?? missions[0]?.id ?? null,
  }
}

export const missionStore = createMissionStore()
