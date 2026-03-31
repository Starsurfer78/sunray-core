export interface Telemetry {
  type: string
  op: string
  x: number
  y: number
  heading: number
  battery_v: number
  charge_v: number
  charge_a: number
  gps_sol: number
  gps_text: string
  gps_acc: number
  gps_lat: number
  gps_lon: number
  bumper_l: boolean
  bumper_r: boolean
  lift: boolean
  motor_err: boolean
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
  mission_id: string
  mission_zone_index: number
  mission_zone_count: number
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
  x: 0,
  y: 0,
  heading: 0,
  battery_v: 0,
  charge_v: 0,
  charge_a: 0,
  gps_sol: 0,
  gps_text: '---',
  gps_acc: 0,
  gps_lat: 0,
  gps_lon: 0,
  bumper_l: false,
  bumper_r: false,
  lift: false,
  motor_err: false,
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
  mission_id: '',
  mission_zone_index: 0,
  mission_zone_count: 0,
}
