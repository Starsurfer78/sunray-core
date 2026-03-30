<script lang="ts">
  import { batteryPercent, telemetry } from '../stores/telemetry'
  import { connection, connectionLabel } from '../stores/connection'
</script>

<div class="statusbar">
  <div class="pill" class:ok={$connection.connected} class:bad={!$connection.connected}>
    {$connectionLabel}
  </div>
  <div class="pill op">{$telemetry.op}</div>
  <div class="pill">Batterie {$telemetry.battery_v.toFixed(1)} V / {$batteryPercent}%</div>
  <div class="pill">Laden {$telemetry.charge_v.toFixed(1)} V</div>
  <div class="pill">GPS {$telemetry.gps_text}</div>
  <div class="pill">MCU {$telemetry.mcu_v || '---'}</div>
</div>

<style>
  .statusbar {
    display: flex;
    flex-wrap: wrap;
    gap: 0.7rem;
  }

  .pill {
    border: 1px solid rgba(183, 214, 190, 0.18);
    border-radius: 999px;
    padding: 0.55rem 0.9rem;
    background: rgba(12, 25, 21, 0.7);
    color: #dce8e8;
  }

  .ok {
    border-color: rgba(86, 197, 150, 0.6);
    color: #8fe7bf;
  }

  .bad {
    border-color: rgba(215, 116, 116, 0.6);
    color: #f4aaaa;
  }

  .op {
    color: #cde886;
  }
</style>
