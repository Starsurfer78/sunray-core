export async function postJson<TResponse, TPayload = Record<string, unknown>>(
  path: string,
  payload: TPayload,
): Promise<TResponse> {
  const response = await fetch(path, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(payload),
  })

  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return response.json() as Promise<TResponse>
}

export async function putJson<TResponse, TPayload = Record<string, unknown>>(
  path: string,
  payload: TPayload,
): Promise<TResponse> {
  const response = await fetch(path, {
    method: 'PUT',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(payload),
  })

  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return response.json() as Promise<TResponse>
}

export interface DiagResponse {
  ok: boolean
  error?: string
  message?: string
  on?: boolean
  ticks?: number
  ticksTarget?: number
  ticksPerRevolution?: number
  revolutions?: number
  motor?: string
  left_ticks?: number
  right_ticks?: number
  mow_ticks?: number
  left_ticks_target?: number
  right_ticks_target?: number
  mow_ticks_target?: number
  distance_m?: number
  distance_target_m?: number
  heading_delta_deg?: number
  target_angle_deg?: number
}

export interface ConfigUpdateResponse {
  ok: boolean
  error?: string
}

export interface FlatConfigDocument {
  ticks_per_revolution?: number
  invert_left_motor?: boolean
  invert_right_motor?: boolean
  dock_auto_start?: boolean
  [key: string]: unknown
}

export interface MapPoint {
  x: number
  y: number
}

export interface MapZone {
  id: string
  order: number
  polygon: MapPoint[]
  settings: {
    name: string
    stripWidth: number
    angle?: number
    edgeMowing?: boolean
    edgeRounds?: number
    speed: number
    pattern: 'stripe' | 'spiral'
    reverseAllowed?: boolean
    clearance?: number
  }
}

export interface MapPlannerSettings {
  defaultClearance?: number
  perimeterSoftMargin?: number
  perimeterHardMargin?: number
  obstacleInflation?: number
  softNoGoCostScale?: number
  replanPeriodMs?: number
  gridCellSize?: number
}

export interface MapDockMeta {
  approachMode?: 'forward_only' | 'reverse_allowed'
  corridor?: Array<[number, number]> | MapPoint[]
  finalAlignHeadingDeg?: number | null
  slowZoneRadius?: number
}

export interface MapExclusionMeta {
  type?: 'hard' | 'soft'
  clearance?: number
  costScale?: number
}

export interface MapDocument {
  perimeter: Array<[number, number]> | MapPoint[]
  dock: Array<[number, number]> | MapPoint[]
  mow: Array<[number, number]> | MapPoint[]
  exclusions: Array<Array<[number, number]>>
  zones?: MapZone[]
  planner?: MapPlannerSettings
  dockMeta?: MapDockMeta
  exclusionMeta?: MapExclusionMeta[]
  captureMeta?: Record<string, unknown>
}

export interface MissionScheduleDocument {
  enabled: boolean
  days: number[]
  startTime: string
  endTime: string
  rainDelayMinutes: number
}

export interface MissionZoneOverridesDocument {
  stripWidth?: number
  angle?: number
  edgeMowing?: boolean
  edgeRounds?: number
  speed?: number
  pattern?: 'stripe' | 'spiral'
}

export interface MissionDocument {
  id: string
  name: string
  zoneIds: string[]
  overrides: Record<string, MissionZoneOverridesDocument>
  schedule?: MissionScheduleDocument
}

export function runMotorDiag(params: {
  motor: 'left' | 'right' | 'mow'
  pwm: number
  duration_ms: number
  revolutions?: number
}) {
  return postJson<DiagResponse>('/api/diag/motor', params)
}

export function runImuCalibration() {
  return postJson<DiagResponse>('/api/diag/imu_calib', {})
}

export function runDriveDiag(params: {
  distance_m: number
  pwm: number
}) {
  return postJson<DiagResponse>('/api/diag/drive', params)
}

export function runTurnDiag(params: {
  angle_deg: number
  pwm: number
}) {
  return postJson<DiagResponse>('/api/diag/turn', params)
}

export function setMowMotor(on: boolean) {
  return postJson<DiagResponse>('/api/diag/mow', { on })
}

export function updateMotorCalibrationConfig(payload: {
  ticks_per_revolution?: number
  invert_left_motor?: boolean
  invert_right_motor?: boolean
}) {
  return putJson<ConfigUpdateResponse>('/api/config', payload)
}

export function updateConfigDocument(payload: FlatConfigDocument) {
  return putJson<ConfigUpdateResponse, FlatConfigDocument>('/api/config', payload)
}

export async function getConfigDocument(): Promise<FlatConfigDocument> {
  const response = await fetch('/api/config')

  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return response.json() as Promise<FlatConfigDocument>
}

export async function getMapDocument(): Promise<MapDocument> {
  const response = await fetch('/api/map')

  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return response.json() as Promise<MapDocument>
}

export function saveMapDocument(payload: Record<string, unknown>) {
  return postJson<ConfigUpdateResponse>('/api/map', payload)
}

export async function getMissions(): Promise<MissionDocument[]> {
  const response = await fetch('/api/missions')
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<MissionDocument[]>
}

export function createMissionDocument(payload: MissionDocument) {
  return postJson<ConfigUpdateResponse, MissionDocument>('/api/missions', payload)
}

export function updateMissionDocument(missionId: string, payload: MissionDocument) {
  return putJson<ConfigUpdateResponse, MissionDocument>(`/api/missions/${encodeURIComponent(missionId)}`, payload)
}

export async function deleteMissionDocument(missionId: string): Promise<ConfigUpdateResponse> {
  const response = await fetch(`/api/missions/${encodeURIComponent(missionId)}`, {
    method: 'DELETE',
  })
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<ConfigUpdateResponse>
}
