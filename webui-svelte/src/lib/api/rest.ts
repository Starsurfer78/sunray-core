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
  name: string
  order: number
  polygon: MapPoint[]
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
  exclusions: Array<Array<[number, number]>>
  zones?: MapZone[]
  planner?: MapPlannerSettings
  dockMeta?: MapDockMeta
  exclusionMeta?: MapExclusionMeta[]
  captureMeta?: Record<string, unknown>
}

export interface StoredMapEntry {
  id: string
  name: string
  file: string
  created_at_ms?: number
  updated_at_ms?: number
}

export interface StoredMapsResponse {
  active_id: string
  maps: StoredMapEntry[]
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
  planRef?: {
    id: string
    revision?: number
    generatedAtMs?: number
  }
}

export interface PlannerPreviewJob {
  source: [number, number]
  destination: [number, number]
  missionMode?: 'perimeter' | 'exclusion' | 'dock' | 'mow' | 'free'
  planningMode?: 'perimeter' | 'exclusion' | 'dock' | 'mow' | 'free'
  headingReferenceRad?: number
  hasHeadingReference?: boolean
  reverseAllowed?: boolean
  clearance_m?: number
  robotRadius_m?: number
}

export type RouteSemantic =
  | 'coverage_edge'
  | 'coverage_infill'
  | 'transit_within_zone'
  | 'transit_between_components'
  | 'transit_inter_zone'
  | 'dock_approach'
  | 'recovery'
  | 'unknown'

export interface PlannerPreviewRoutePoint {
  p: [number, number]
  reverse: boolean
  slow: boolean
  reverseAllowed: boolean
  clearance_m: number
  sourceMode: 'perimeter' | 'exclusion' | 'dock' | 'mow' | 'free'
  semantic: RouteSemantic
  zoneId: string
  componentId: string
}

export interface RouteValidationError {
  pointIndex: number
  code: number
  message: string
  zoneId: string
}

/** N3.3: single waypoint entry in the pre-computed flat mission sequence. */
export interface WaypointEntry {
  x: number
  y: number
  mowOn: boolean
}

export interface PlannerPreviewRoute {
  index: number
  ok: boolean
  valid?: boolean
  error?: string
  planRef?: {
    id: string
    revision?: number
    generatedAtMs?: number
  }
  plan?: {
    missionId: string
    zoneOrder: string[]
    valid: boolean
    invalidReason: string
    zonePlans: Array<{
      zoneId: string
      zoneName: string
      valid: boolean
      invalidReason: string
    }>
  }
  validationErrors?: RouteValidationError[]
  debug?: {
    pointCount: number
    active: boolean
    valid: boolean
    invalidReason: string
    semanticCounts: Record<string, number>
    zoneOrder: string[]
    componentOrder: Array<{
      componentId: string
      zoneId: string
      firstSemantic: string
    }>
  }
  route?: {
    active: boolean
    valid: boolean
    invalidReason: string
    sourceMode: 'perimeter' | 'exclusion' | 'dock' | 'mow' | 'free'
    points: PlannerPreviewRoutePoint[]
  }
  /** N3.3: flat ordered waypoint sequence including inter-zone transit segments. */
  waypoints?: WaypointEntry[]
}

export interface PlannerPreviewRequest {
  map: MapDocument
  mission?: MissionDocument
  jobs?: PlannerPreviewJob[]
}

export interface PlannerPreviewResponse {
  ok: boolean
  routes: PlannerPreviewRoute[]
  error?: string
}

export async function previewPlannerRoutes(
  payload: PlannerPreviewRequest,
): Promise<PlannerPreviewResponse> {
  return postJson<PlannerPreviewResponse, PlannerPreviewRequest>('/api/planner/preview', payload)
}

export interface HistoryEventItem {
  ts_ms: number
  wall_ts_ms: number
  level: string
  module: string
  event_type: string
  state_phase: string
  event_reason: string
  error_code?: string
  message: string
  battery_v?: number
  gps_sol?: number
  x?: number
  y?: number
}

export interface HistorySessionItem {
  id: string
  started_at_ms: number
  ended_at_ms?: number
  duration_ms: number
  distance_m: number
  battery_start_v: number
  battery_end_v: number
  end_reason: string
  mean_gps_accuracy_m?: number
  max_gps_accuracy_m?: number
  metadata?: Record<string, unknown>
}

export interface HistoryListResponse<TItem> {
  ok: boolean
  backend_ready: boolean
  items: TItem[]
  limit: number
}

export interface HistorySummaryResponse {
  ok: boolean
  backend_ready: boolean
  events_total: number
  event_reason_counts: Record<string, number>
  event_type_counts: Record<string, number>
  event_level_counts: Record<string, number>
  sessions_total: number
  sessions_completed: number
  mowing_duration_ms_total: number
  mowing_distance_m_total: number
  last_event_ts_ms: number
  last_event_wall_ts_ms: number
  last_session_started_at_ms: number
  retention: {
    max_events: number
    max_sessions: number
  }
  export_enabled: boolean
  database_path: string
  database_exists: boolean
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

export async function exportMapGeoJson(): Promise<Blob> {
  const response = await fetch('/api/map/geojson')

  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return response.blob()
}

export function importMapGeoJson(payload: string) {
  return postJson<ConfigUpdateResponse, unknown>('/api/map/geojson', JSON.parse(payload))
}

export async function getStoredMaps(): Promise<StoredMapsResponse> {
  const response = await fetch('/api/maps')
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<StoredMapsResponse>
}

export function createStoredMap(payload: { name: string; activate?: boolean; map?: Record<string, unknown> }) {
  return postJson<ConfigUpdateResponse & { id?: string; active?: boolean }, typeof payload>('/api/maps', payload)
}

export function activateStoredMap(id: string) {
  return postJson<ConfigUpdateResponse>(`/api/maps/${encodeURIComponent(id)}/activate`, {})
}

export async function deleteStoredMap(id: string): Promise<ConfigUpdateResponse> {
  const response = await fetch(`/api/maps/${encodeURIComponent(id)}`, { method: 'DELETE' })
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<ConfigUpdateResponse>
}

export async function getMissions(): Promise<MissionDocument[]> {
  const response = await fetch('/api/missions')
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<MissionDocument[]>
}

export async function getHistoryEvents(limit = 50): Promise<HistoryListResponse<HistoryEventItem>> {
  const response = await fetch(`/api/history/events?limit=${encodeURIComponent(String(limit))}`)
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<HistoryListResponse<HistoryEventItem>>
}

export async function getHistorySessions(limit = 20): Promise<HistoryListResponse<HistorySessionItem>> {
  const response = await fetch(`/api/history/sessions?limit=${encodeURIComponent(String(limit))}`)
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<HistoryListResponse<HistorySessionItem>>
}

export async function getStatisticsSummary(): Promise<HistorySummaryResponse> {
  const response = await fetch('/api/statistics/summary')
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<HistorySummaryResponse>
}

export function clearHistoryDb(): Promise<ConfigUpdateResponse> {
  return postJson<ConfigUpdateResponse>('/api/history/clear', {})
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

// ── OTA ────────────────────────────────────────────────────────────────────────

export interface OtaCheckResponse {
  status: 'up_to_date' | 'update_available' | 'error' | 'unknown'
  hash?: string
  detail?: string
}

export async function otaCheck(): Promise<OtaCheckResponse> {
  const response = await fetch('/api/ota/check', { method: 'POST' })
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<OtaCheckResponse>
}

export async function otaUpdate(): Promise<{ status: string }> {
  const response = await fetch('/api/ota/update', { method: 'POST' })
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || `${response.status} ${response.statusText}`)
  }
  return response.json() as Promise<{ status: string }>
}

export interface StmProbeResponse {
  ok: boolean
  status: 'probe_ok' | 'probe_failed' | 'probe_tool_missing'
  detail: string
}

export async function stmProbe(): Promise<StmProbeResponse> {
  const response = await fetch('/api/stm/probe', { method: 'POST' })
  const text = await response.text()

  if (!response.ok) {
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return JSON.parse(text) as StmProbeResponse
}

export interface StmUploadedFirmwareInfo {
  ok: boolean
  exists: boolean
  original_name?: string
  size_bytes?: number
  uploaded_at_ms?: number
  stored_path?: string
}

export interface StmFlashResponse {
  ok: boolean
  status: 'flash_ok' | 'flash_failed' | 'flash_busy'
  detail: string
}

export async function getStmUploadedFirmware(): Promise<StmUploadedFirmwareInfo> {
  const response = await fetch('/api/stm/uploaded')
  const text = await response.text()

  if (!response.ok) {
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return JSON.parse(text) as StmUploadedFirmwareInfo
}

export async function uploadStmFirmware(file: File): Promise<StmUploadedFirmwareInfo> {
  const response = await fetch('/api/stm/upload', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/octet-stream',
      'X-File-Name': file.name,
    },
    body: file,
  })
  const text = await response.text()

  if (!response.ok) {
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return JSON.parse(text) as StmUploadedFirmwareInfo
}

export async function flashUploadedStm(): Promise<StmFlashResponse> {
  const response = await fetch('/api/stm/flash', { method: 'POST' })
  const text = await response.text()

  if (!response.ok) {
    throw new Error(text || `${response.status} ${response.statusText}`)
  }

  return JSON.parse(text) as StmFlashResponse
}

export async function restartService(): Promise<{ status: string }> {
  const response = await fetch('/api/restart', { method: 'POST' })
  const text = await response.text()
  if (!response.ok) throw new Error(text || `${response.status} ${response.statusText}`)
  return JSON.parse(text) as { status: string }
}

// N4.2: Active mission plan (geometry + live index) ----------------------------

export interface ActivePlanResponse {
  missionId: string
  waypoints: WaypointEntry[]
  zoneOrder: string[]
  planRef?: {
    id: string
    revision?: number
    generatedAtMs?: number
  }
  route?: {
    active: boolean
    valid: boolean
    invalidReason: string
    sourceMode: 'perimeter' | 'exclusion' | 'dock' | 'mow' | 'free'
    points: PlannerPreviewRoutePoint[]
  }
  waypointIndex: number
  waypointTotal: number
  activePointIndex?: number
}

export async function getActivePlan(): Promise<ActivePlanResponse> {
  const response = await fetch('/api/mission/active-plan')
  if (!response.ok) throw new Error(`getActivePlan: ${response.status}`)
  return response.json() as Promise<ActivePlanResponse>
}

// N5.1/N5.2: Live map overlay (obstacles + active detour) --------------------

export interface ObstacleEntry {
  x: number
  y: number
  r: number    // radius in meters
  hits: number
}

export interface DetourEntry {
  active: boolean
  waypoints: { x: number; y: number }[]
  reentryX?: number
  reentryY?: number
}

export interface LiveOverlayResponse {
  obstacles: ObstacleEntry[]
  detour: DetourEntry
}

export async function getLiveOverlay(): Promise<LiveOverlayResponse> {
  const response = await fetch('/api/map/live')
  if (!response.ok) throw new Error(`getLiveOverlay: ${response.status}`)
  return response.json() as Promise<LiveOverlayResponse>
}
