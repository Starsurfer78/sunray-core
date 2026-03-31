<script lang="ts">
  import { batteryPercent, telemetry } from '../stores/telemetry'
  import { connection, connectionLabel } from '../stores/connection'

  export let compact = false

  const formatNumber = (value: number | string | null | undefined, digits = 1) =>
    typeof value === 'number' ? value.toFixed(digits) : '---'

  const formatText = (value: string | null | undefined) =>
    typeof value === 'string' && value.trim().length > 0 ? value : '---'

  $: isChargingState = $telemetry.op === 'Charge' || $telemetry.state_phase === 'charging'
  $: chargeStateLabel =
    isChargingState && $telemetry.charge_a > 0.05
      ? `Laedt ${formatNumber($telemetry.charge_a, 2)} A`
      : $telemetry.charge_v >= 7
        ? `Dockkontakt ${formatNumber($telemetry.charge_v)} V`
        : 'Kein Dock'
</script>

<div class:compact class="statusbar">
  <div class="pill conn" class:ok={$connection.connected} class:bad={!$connection.connected}>
    <span class="dot"></span>
    {$connectionLabel}
  </div>

  <div class="pill op">
    {$telemetry.op || 'idle'}
  </div>

  <div class="pill">
    <span class="label">Akku</span>
    <strong>{formatNumber($telemetry.battery_v)} V</strong>
    <span class="sub">{$batteryPercent}%</span>
  </div>

  <div class="pill">
    <span class="label">Dock</span>
    <strong>{chargeStateLabel}</strong>
  </div>

  <div class="pill">
    <span class="label">GPS</span>
    <strong>{$telemetry.gps_text || '---'}</strong>
  </div>

  <div class="pill">
    <span class="label">MCU</span>
    <strong>{formatText($telemetry.mcu_v)}</strong>
  </div>
</div>

<style>
  .statusbar {
    display: flex;
    flex-wrap: wrap;
    gap: 0.55rem;
    min-width: 0;
  }

  .pill {
    display: inline-flex;
    align-items: center;
    gap: 0.38rem;
    min-width: 0;
    border: 1px solid #1e3a5f;
    border-radius: 999px;
    padding: 0.42rem 0.74rem;
    background: #0f1829;
    color: #dce8e8;
    font-size: 0.78rem;
    line-height: 1;
  }

  .pill strong {
    font-size: 0.8rem;
    color: inherit;
  }

  .pill .label {
    color: #7a8da8;
  }

  .pill .sub {
    color: #c4b5fd;
  }

  .conn {
    color: #94a3b8;
  }

  .ok {
    border-color: #166534;
    background: #052e16;
    color: #4ade80;
  }

  .bad {
    border-color: #7f1d1d;
    background: #3b0b12;
    color: #fca5a5;
  }

  .op {
    background: #0c1a3a;
    border-color: #1e40af;
    color: #93c5fd;
  }

  .dot {
    width: 0.38rem;
    height: 0.38rem;
    border-radius: 999px;
    background: currentColor;
    flex-shrink: 0;
  }

  .compact .pill {
    padding: 0.34rem 0.62rem;
    font-size: 0.74rem;
  }

  .compact .pill strong {
    font-size: 0.76rem;
  }
</style>
