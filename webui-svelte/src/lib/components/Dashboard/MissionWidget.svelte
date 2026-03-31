<script lang="ts">
  import { get } from "svelte/store";
  import { onMount } from "svelte";
  import { getMissions, type MissionDocument } from "../../api/rest";
  import { sendCmd } from "../../api/websocket";
  import { connection } from "../../stores/connection";
  import { missionStore, type Mission } from "../../stores/missions";
  import { telemetry } from "../../stores/telemetry";
  import type { Zone } from "../../stores/map";

  export let zones: Zone[] = [];

  let missions: Mission[] = [];
  let selectedStartMissionId = "";
  let loadInfo = "";

  function normalizeMission(doc: MissionDocument): Mission {
    return {
      id: doc.id,
      name: doc.name,
      zoneIds: doc.zoneIds ?? [],
      overrides: doc.overrides ?? {},
      schedule: doc.schedule,
    };
  }

  function zoneName(zoneId: string): string {
    return zones.find((zone) => zone.id === zoneId)?.settings.name ?? zoneId;
  }

  function toMinutes(time: string): number {
    const [hour, minute] = time.split(":").map((value) => Number(value));
    return hour * 60 + minute;
  }

  function upcomingMissionCandidate(
    mission: Mission,
    now: Date,
  ): { mission: Mission; time: Date } | null {
    if (!mission.schedule?.enabled || mission.schedule.days.length === 0)
      return null;

    const nowDay = now.getDay();
    const nowMinutes = now.getHours() * 60 + now.getMinutes();
    const startMinutes = toMinutes(mission.schedule.startTime);

    for (let offset = 0; offset < 7; offset += 1) {
      const day = (nowDay + offset) % 7;
      if (!mission.schedule.days.includes(day)) continue;
      const candidate = new Date(now);
      candidate.setDate(now.getDate() + offset);
      candidate.setHours(
        Math.floor(startMinutes / 60),
        startMinutes % 60,
        0,
        0,
      );
      if (offset > 0 || startMinutes >= nowMinutes) {
        return { mission, time: candidate };
      }
    }
    return null;
  }

  function formatWhen(date: Date): string {
    return date.toLocaleString("de-DE", {
      weekday: "short",
      hour: "2-digit",
      minute: "2-digit",
    });
  }

  async function loadMissionData() {
    try {
      const docs = await getMissions();
      missions = docs.map((mission) => normalizeMission(mission));
      missionStore.replaceAll(missions, zones);
      loadInfo = "Missionen vom Server";
    } catch {
      missions = get(missionStore).missions;
      loadInfo =
        missions.length > 0 ? "Lokale Missionen" : "Noch keine Mission";
    }

    if (!selectedStartMissionId && missions.length > 0) {
      selectedStartMissionId = missions[0].id;
    }
  }

  function startSelectedMission() {
    if (!selectedStartMissionId) return;
    sendCmd("start", { missionId: selectedStartMissionId });
  }

  onMount(() => {
    void loadMissionData();
  });

  $: if (zones.length > 0 && missions.length === 0) {
    missions = get(missionStore).missions;
  }

  $: nextMission = (() => {
    const now = new Date();
    const candidates = missions
      .map((mission) => upcomingMissionCandidate(mission, now))
      .filter((entry): entry is { mission: Mission; time: Date } =>
        Boolean(entry),
      )
      .sort((left, right) => left.time.getTime() - right.time.getTime());
    return candidates[0] ?? null;
  })();

  $: runningMission = $telemetry.mission_id
    ? (missions.find((mission) => mission.id === $telemetry.mission_id) ?? null)
    : null;

  $: runningZoneName =
    runningMission && $telemetry.mission_zone_index > 0
      ? zoneName(
          runningMission.zoneIds[$telemetry.mission_zone_index - 1] ?? "",
        )
      : "";

  $: runningProgress =
    $telemetry.mission_zone_count > 0
      ? Math.max(
          0,
          Math.min(
            100,
            Math.round(
              ($telemetry.mission_zone_index / $telemetry.mission_zone_count) *
                100,
            ),
          ),
        )
      : 0;

  $: missionIsRunning =
    Boolean(runningMission) &&
    ["Undock", "NavToStart", "Mow"].includes($telemetry.op);
</script>

<section class="panel mission-panel">
  <div class="section-title">Mission</div>

  {#if missionIsRunning && runningMission}
    <div class="mission-card">
      <div class="card-title">Mission läuft</div>
      <strong>{runningMission.name}</strong>
      <div class="card-meta">
        Zone {$telemetry.mission_zone_index} / {$telemetry.mission_zone_count}
        {#if runningZoneName}
          · {runningZoneName}
        {/if}
      </div>
      <div class="progress" aria-label={`Fortschritt ${runningProgress}%`}>
        <span class="progress-fill" style={`width:${runningProgress}%`}></span>
      </div>
      <button
        type="button"
        class="secondary-btn"
        on:click={() => sendCmd("stop")}
        disabled={!$connection.connected}
      >
        Abbrechen
      </button>
    </div>
  {:else if nextMission}
    <div class="mission-card">
      <div class="card-title">Nächste Mission</div>
      <strong>{nextMission.mission.name}</strong>
      <div class="card-meta">
        {formatWhen(nextMission.time)} · {nextMission.mission.zoneIds.length} Zonen
      </div>
      <label class="mission-select">
        <span>Jetzt starten</span>
        <select bind:value={selectedStartMissionId}>
          {#each missions as mission}
            <option value={mission.id}>{mission.name}</option>
          {/each}
        </select>
      </label>
      <button
        type="button"
        class="primary-btn"
        on:click={startSelectedMission}
        disabled={!$connection.connected || !selectedStartMissionId}
      >
        Mission starten
      </button>
    </div>
  {:else}
    <div class="mission-card empty">
      <div class="card-title">Mission</div>
      <strong>Noch keine geplante Mission</strong>
      <div class="card-meta">{loadInfo}</div>
    </div>
  {/if}
</section>

<style>
  .mission-panel {
    display: grid;
    gap: 0.4rem;
    padding: 0.58rem 0.68rem;
    border-bottom: 1px solid #0f1829;
  }

  .mission-card {
    display: grid;
    gap: 0.45rem;
    padding: 0;
  }

  .card-title {
    color: #60a5fa;
    font-size: 0.66rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .card-meta {
    color: #94a3b8;
    font-size: 0.72rem;
  }

  .mission-select {
    display: grid;
    gap: 0.25rem;
    color: #94a3b8;
    font-size: 0.72rem;
  }

  .mission-select select,
  .primary-btn,
  .secondary-btn {
    width: 100%;
    padding: 0.6rem 0.75rem;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #0a1020;
    color: #dbeafe;
  }

  .primary-btn {
    border-color: #2563eb;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 700;
    cursor: pointer;
  }

  .secondary-btn {
    border-color: #7f1d1d;
    color: #fca5a5;
    cursor: pointer;
  }

  .primary-btn:disabled,
  .secondary-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .progress {
    height: 0.5rem;
    border-radius: 999px;
    background: #09111f;
    overflow: hidden;
  }

  .progress-fill {
    display: block;
    height: 100%;
    background: linear-gradient(90deg, #2563eb, #22d3ee);
  }

  .empty {
    opacity: 0.85;
  }
</style>
