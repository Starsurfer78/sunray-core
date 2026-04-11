<script lang="ts">
  import { onMount } from "svelte";
  import MissionWidget from "./MissionWidget.svelte";
  import DockSettingsCard from "./DockSettingsCard.svelte";
  import RobotControls from "../RobotControls.svelte";
  import OtaUpdate from "../OtaUpdate.svelte";
  import { batteryPercent, telemetry } from "../../stores/telemetry";
  import { connection } from "../../stores/connection";
  import { mapStore } from "../../stores/map";
  import { missionStore } from "../../stores/missions";
  import { getPreflightChecks } from "../../utils/robotUi";
  import { RAD_TO_DEG } from "../../utils/mapHelpers";

  let nowMs = Date.now();

  const RTCM_WARN_MS = 5000;

  $: preflight = getPreflightChecks(
    $telemetry,
    $connection,
    $mapStore.map,
    nowMs,
  );
  $: startAllowed = preflight.every((c) => c.ok);
  $: startHint = preflight.find((c) => !c.ok)?.hint ?? "";
  // Laufende Mission hat Vorrang, sonst erste verfügbare Mission
  $: activeMissionId =
    $telemetry.mission_id || $missionStore.missions[0]?.id || "";
  $: headingDeg =
    ((Math.round(($telemetry.heading ?? 0) * RAD_TO_DEG) % 360) + 360) % 360;

  $: gpsLabel =
    $telemetry.gps_sol === 4
      ? "RTK-Fix"
      : $telemetry.gps_sol === 5
        ? "RTK-Float"
        : $telemetry.gps_sol >= 1
          ? "GPS"
          : "Kein Signal";
  $: gpsOk = $telemetry.gps_sol >= 4;
  $: gpsSolutionLabel =
    $telemetry.gps_sol > 0 ? `sol: ${$telemetry.gps_sol}` : "sol: —";
  $: gpsStatusLabel = $telemetry.gps_text || "---";
  $: gpsAccuracyLabel =
    $telemetry.gps_acc > 0 ? $telemetry.gps_acc.toFixed(2) : "—";
  $: gpsRoverSvLabel =
    $telemetry.gps_num_sv > 0 ? String($telemetry.gps_num_sv) : "—";
  $: gpsHasRtcm = $telemetry.gps_dgps_age_ms > 0 || $telemetry.gps_sol >= 4;
  $: gpsRtcmFresh =
    gpsHasRtcm && $telemetry.gps_dgps_age_ms <= RTCM_WARN_MS;
  $: gpsCorrectedSignalsLabel =
    $telemetry.gps_num_corr_signals > 0 ? String($telemetry.gps_num_corr_signals) : "0";
  $: gpsRtcmAgeLabel =
    $telemetry.gps_dgps_age_ms > 0
      ? ($telemetry.gps_dgps_age_ms / 1000).toFixed(1)
      : "—";
  $: gpsRtcmStateLabel = gpsRtcmFresh
    ? "frisch"
    : gpsHasRtcm
      ? "alt"
      : "nie";
  $: gpsRtcmAgeWarn =
    gpsHasRtcm && $telemetry.gps_dgps_age_ms > RTCM_WARN_MS;
  $: gpsCorrectedSignalsWarn =
    !gpsRtcmFresh && $telemetry.gps_sol >= 4 && $telemetry.gps_num_corr_signals <= 0;
  $: gpsQualityWarnings = [
    ...(!gpsHasRtcm ? ["Keine RTCM-Nachrichten gesehen"] : []),
    ...(gpsRtcmAgeWarn ? [`RTCM veraltet (${gpsRtcmAgeLabel}s)`] : []),
    ...(gpsCorrectedSignalsWarn ? ["Keine korrigierten Signale"] : []),
  ];
  $: gpsQualityWarn = gpsQualityWarnings.length > 0;
  $: gpsQualityBanner = gpsQualityWarn
    ? gpsQualityWarnings.join(" • ")
    : gpsOk
      ? "RTK-Qualität stabil"
      : "RTK noch nicht stabil";
  $: gpsLatLabel = formatCoordinate($telemetry.gps_lat, "lat");
  $: gpsLonLabel = formatCoordinate($telemetry.gps_lon, "lon");

  $: batPct = $batteryPercent;
  $: batBarW = `${Math.max(0, Math.min(100, batPct))}%`;

  $: sensors = [
    { label: "Bumper L", ok: !$telemetry.bumper_l },
    { label: "Bumper R", ok: !$telemetry.bumper_r },
    { label: "Lift", ok: !$telemetry.lift },
    { label: "Motor", ok: !$telemetry.motor_err },
  ];

  $: zoneLabel =
    $telemetry.mission_zone_count > 0
      ? `Zone ${$telemetry.mission_zone_index} / ${$telemetry.mission_zone_count}`
      : "—";

  function formatCoordinate(value: number, axis: "lat" | "lon") {
    if (!value) return "—";
    const suffix =
      axis === "lat" ? (value >= 0 ? "N" : "S") : value >= 0 ? "E" : "W";
    return `${Math.abs(value).toFixed(8)}°${suffix}`;
  }

  onMount(() => {
    const interval = setInterval(() => {
      nowMs = Date.now();
    }, 1000);
    return () => clearInterval(interval);
  });
</script>

<div class="sb-content">
  <!-- Preflight compact -->
  <div
    class="preflight-bar"
    class:ok={startAllowed}
    class:blocked={!startAllowed}
  >
    <span class="pf-dot"></span>
    <span class="pf-label"
      >{startAllowed ? "Startfreigabe erteilt" : startHint}</span
    >
  </div>

  <!-- Status -->
  <div class="sr-sec">
    <div class="sr-lbl">Status</div>
    <div class="sr-stbig">{$telemetry.op}</div>
    <div class="sr-stsub">{zoneLabel}</div>
    <div class="sr-pos">
      X {$telemetry.x.toFixed(2)} m &nbsp; Y {$telemetry.y.toFixed(2)} m &nbsp; {headingDeg}°
    </div>
  </div>

  <!-- GPS -->
  <div class="sr-sec">
    <div class="sr-lbl">GPS</div>
    <div class="sr-gps">
      <div class="sr-gps-top">
        <div class="sr-fix" class:ok={gpsOk} class:warn={!gpsOk}>
          <span class="sr-fixdot"></span>{gpsLabel}
        </div>
        <span class="sr-sol">{gpsSolutionLabel}</span>
      </div>
      <div
        class="sr-gps-banner"
        class:warn={gpsQualityWarn}
        class:ok={!gpsQualityWarn && gpsOk}
      >
        {gpsQualityBanner}
      </div>
      <div class="sr-satrow">
        <div class="sr-sat">
          <div class="sr-satlbl">Rover</div>
          <div class="sr-satval">{gpsRoverSvLabel}</div>
          <div class="sr-satsub">Satelliten</div>
        </div>
        <div class="sr-sat" class:warn={gpsCorrectedSignalsWarn}>
          <div class="sr-satlbl">Korrigiert</div>
          <div class="sr-satval">{gpsCorrectedSignalsLabel}</div>
          <div class="sr-satsub">Signale</div>
        </div>
        <div class="sr-sat" class:warn={gpsRtcmAgeWarn}>
          <div class="sr-satlbl">Genauigkeit</div>
          <div class="sr-satval">{gpsAccuracyLabel}</div>
          <div class="sr-satsub">Meter</div>
        </div>
      </div>
      <div class="sr-gps-meta">
        <span class="sr-gps-meta-item">{gpsStatusLabel}</span>
        <span class="sr-gps-meta-item" class:warn={gpsRtcmAgeWarn}
          >RTCM {gpsRtcmStateLabel} · {gpsRtcmAgeLabel}s</span
        >
      </div>
      <div class="sr-coords">{gpsLatLabel}<br />{gpsLonLabel}</div>
    </div>
  </div>

  <!-- Akku -->
  <div class="sr-sec">
    <div class="sr-lbl">Akku</div>
    <div class="sr-sg">
      <div class="sr-sc">
        <div class="sr-slbl">Spannung</div>
        <div class="sr-sval blue">{$telemetry.battery_v.toFixed(1)} V</div>
        <div class="sr-bar">
          <div class="sr-barf" style="width:{batBarW}"></div>
        </div>
        <div class="sr-satsub">{batPct}%</div>
      </div>
      <div class="sr-sc">
        <div class="sr-slbl">Laden</div>
        <div class="sr-sval {$telemetry.charge_v > 1 ? 'amber' : 'muted'}">
          {$telemetry.charge_v.toFixed(1)} V
        </div>
        {#if $telemetry.charge_a > 0}
          <div class="sr-satsub">{$telemetry.charge_a.toFixed(2)} A</div>
        {/if}
      </div>
    </div>
  </div>

  <!-- Sensoren -->
  <div class="sr-sec">
    <div class="sr-lbl">Sensoren</div>
    <div class="sr-sensg">
      {#each sensors as s}
        <div class="sr-sens">
          <span class={s.ok ? "dok" : "derr"}></span>{s.label}
        </div>
      {/each}
    </div>
  </div>

  <!-- Steuerung -->
  <div class="sr-sec">
    <RobotControls {startAllowed} {startHint} missionId={activeMissionId} />
  </div>

  <MissionWidget zones={$mapStore.map.zones} />

  <DockSettingsCard />

  <OtaUpdate />
</div>

<style>
  .sb-content {
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow-y: auto;
    overflow-x: hidden;
  }

  /* Preflight compact bar */
  .preflight-bar {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px;
    font-size: 10px;
    border-bottom: 1px solid #0f1829;
    flex-shrink: 0;
  }
  .preflight-bar.ok {
    background: #052e16;
    color: #4ade80;
  }
  .preflight-bar.blocked {
    background: #3b2305;
    color: #fbbf24;
  }
  .pf-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: currentColor;
    flex-shrink: 0;
  }
  .pf-label {
    line-height: 1.3;
  }

  /* Sections */
  .sr-sec {
    padding: 10px 12px;
    border-bottom: 1px solid #0f1829;
    flex-shrink: 0;
  }
  .sr-lbl {
    font-size: 10px;
    font-weight: 500;
    color: #475569;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 7px;
  }
  .sr-stbig {
    font-size: 18px;
    font-weight: 500;
    color: #60a5fa;
  }
  .sr-stsub {
    font-size: 11px;
    color: #64748b;
    margin-top: 2px;
  }
  .sr-pos {
    font-family: monospace;
    font-size: 10px;
    color: #64748b;
    margin-top: 4px;
  }

  /* GPS */
  .sr-gps {
    background: #0f1829;
    border-radius: 6px;
    padding: 8px 9px;
    border: 1px solid #1e3a5f;
  }
  .sr-gps-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 7px;
  }
  .sr-gps-banner {
    font-size: 10px;
    font-weight: 500;
    border-radius: 4px;
    padding: 5px 7px;
    margin-bottom: 6px;
    background: #0a1020;
    color: #64748b;
    border: 1px solid #1e3a5f;
  }
  .sr-gps-banner.ok {
    color: #4ade80;
    border-color: #14532d;
    background: #052e16;
  }
  .sr-gps-banner.warn {
    color: #fbbf24;
    border-color: #78350f;
    background: #3b2305;
  }
  .sr-fix {
    font-size: 12px;
    font-weight: 500;
    display: flex;
    align-items: center;
    gap: 5px;
  }
  .sr-fix.ok {
    color: #4ade80;
  }
  .sr-fix.warn {
    color: #fbbf24;
  }
  .sr-fixdot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: currentColor;
    flex-shrink: 0;
  }
  .sr-sol {
    font-size: 10px;
    color: #475569;
    font-family: monospace;
    background: #0a1020;
    padding: 2px 5px;
    border-radius: 3px;
  }
  .sr-satrow {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 5px;
    margin-bottom: 6px;
  }
  .sr-sat {
    background: #070d18;
    border-radius: 4px;
    padding: 6px 8px;
    border: 1px solid #1e3a5f;
  }
  .sr-sat.warn {
    border-color: #78350f;
    background: #211406;
  }
  .sr-satlbl {
    font-size: 10px;
    color: #64748b;
    margin-bottom: 2px;
  }
  .sr-satval {
    font-size: 15px;
    font-weight: 500;
    color: #60a5fa;
    font-family: monospace;
  }
  .sr-satsub {
    font-size: 10px;
    color: #475569;
  }
  .sr-gps-meta {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 6px;
    margin-bottom: 6px;
  }
  .sr-gps-meta-item {
    font-size: 10px;
    color: #64748b;
    font-family: monospace;
    background: #0a1020;
    padding: 2px 6px;
    border-radius: 4px;
  }
  .sr-gps-meta-item.warn {
    color: #fbbf24;
    background: #211406;
  }
  .sr-coords {
    font-size: 10px;
    color: #475569;
    font-family: monospace;
    line-height: 1.6;
  }

  /* Akku */
  .sr-sg {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 5px;
  }
  .sr-sc {
    background: #0f1829;
    border-radius: 5px;
    padding: 7px 9px;
    border: 1px solid #1a2a40;
  }
  .sr-slbl {
    font-size: 10px;
    color: #475569;
    margin-bottom: 2px;
  }
  .sr-sval {
    font-size: 14px;
    font-weight: 500;
  }
  .blue {
    color: #60a5fa;
  }
  .amber {
    color: #fbbf24;
  }
  .muted {
    color: #475569;
  }
  .sr-bar {
    height: 3px;
    background: #0a1020;
    border-radius: 2px;
    overflow: hidden;
    margin-top: 4px;
  }
  .sr-barf {
    height: 100%;
    background: #2563eb;
    border-radius: 2px;
  }

  /* Sensoren */
  .sr-sensg {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 4px;
  }
  .sr-sens {
    background: #0f1829;
    border-radius: 4px;
    padding: 5px 7px;
    font-size: 11px;
    display: flex;
    align-items: center;
    gap: 5px;
    color: #94a3b8;
    border: 1px solid #1a2a40;
  }
  .dok {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: #166534;
    border: 1.5px solid #4ade80;
    flex-shrink: 0;
  }
  .derr {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: #450a0a;
    border: 1.5px solid #f87171;
    flex-shrink: 0;
  }
</style>
