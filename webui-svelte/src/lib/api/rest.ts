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
}

export interface ConfigUpdateResponse {
  ok: boolean
  error?: string
}

export interface FlatConfigDocument {
  ticks_per_revolution?: number
  invert_left_motor?: boolean
  invert_right_motor?: boolean
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
  }
}

export interface MapDocument {
  perimeter: Array<[number, number]> | MapPoint[]
  dock: Array<[number, number]> | MapPoint[]
  mow: Array<[number, number]> | MapPoint[]
  exclusions: Array<Array<[number, number]>>
  zones?: MapZone[]
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
