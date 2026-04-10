export interface Telemetry {
  type: string
  op: string
  runtime_health: string
  x: number
  y: number
  heading: number
  battery_v: number
  charge_v: number
  charge_a: number
  charger_connected: boolean
  gps_sol: number
  gps_text: string
  gps_acc: number
  gps_num_sv: number
  gps_num_corr_signals: number
  gps_dgps_age_ms: number
  gps_lat: number
  gps_lon: number
  bumper_l: boolean
  bumper_r: boolean
  stop_button: boolean
  lift: boolean
  motor_err: boolean
  mow_fault_pin: boolean
  mow_overload: boolean
  mow_permanent_fault: boolean
  mow_ov_check: boolean
  uptime_s: number
  diag_active: boolean
  diag_ticks: number
  mcu_v: string
  pi_v: string
  imu_h: number
  imu_r: number
  imu_p: number
  ekf_health: string
  state_phase: string
  resume_target: string
  event_reason: string
  error_code: string
  history_backend_ready: boolean
  mission_id: string
  mission_zone_index: number
  mission_zone_count: number
  waypoint_index: number   // N4.1: current point index in active plan (-1 = inactive)
  waypoint_total: number   // N4.1: total points in active plan
  target_x: number         // N4.3: current navigation target X (world m)
  target_y: number         // N4.3: current navigation target Y (world m)
  has_target: boolean      // N4.3: true when target_x/y is valid
  has_interrupted_mission: boolean // N6.3: a plan was saved when docking interrupted a mission
  ui_message: string  // transient notification text sent by the backend (empty when inactive)
  ui_severity: string // 'info' | 'warn' | 'error'
}

export interface TelemetryEnvelope extends Telemetry {
  type: 'state'
}

export interface LogEnvelope {
  type: 'log'
  text: string
}

export type WsMessage = TelemetryEnvelope | LogEnvelope

export const defaultTelemetry: Telemetry = {
  type: '',
  op: 'Idle',
  runtime_health: 'ok',
  x: 0,
  y: 0,
  heading: 0,
  battery_v: 0,
  charge_v: 0,
  charge_a: 0,
  charger_connected: false,
  gps_sol: 0,
  gps_text: '---',
  gps_acc: 0,
  gps_num_sv: 0,
  gps_num_corr_signals: 0,
  gps_dgps_age_ms: 0,
  gps_lat: 0,
  gps_lon: 0,
  bumper_l: false,
  bumper_r: false,
  stop_button: false,
  lift: false,
  motor_err: false,
  mow_fault_pin: false,
  mow_overload: false,
  mow_permanent_fault: false,
  mow_ov_check: false,
  uptime_s: 0,
  diag_active: false,
  diag_ticks: 0,
  mcu_v: '',
  pi_v: '',
  imu_h: 0,
  imu_r: 0,
  imu_p: 0,
  ekf_health: 'Odo',
  state_phase: 'idle',
  resume_target: '',
  event_reason: 'none',
  error_code: '',
  history_backend_ready: false,
  mission_id: '',
  mission_zone_index: 0,
  mission_zone_count: 0,
  waypoint_index: -1,
  waypoint_total: 0,
  target_x: 0,
  target_y: 0,
  has_target: false,
  has_interrupted_mission: false,
  ui_message: '',
  ui_severity: '',
}
