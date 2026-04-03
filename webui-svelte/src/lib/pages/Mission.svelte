<script lang="ts">
  import { get } from "svelte/store";
  import { onMount } from "svelte";
  import PathPreview from "../components/Mission/PathPreview.svelte";
  import ZoneSettings from "../components/Mission/ZoneSettings.svelte";
  import PageLayout from "../components/PageLayout.svelte";
  import BottomPanel from "../components/Dashboard/BottomPanel.svelte";
  import {
    createMissionDocument,
    deleteMissionDocument,
    getMapDocument,
    getMissions,
    updateMissionDocument,
    type MapZone,
    type MissionDocument,
  } from "../api/rest";
  import { mapStore, type Point, type Zone } from "../stores/map";
  import { missionStore, type Mission } from "../stores/missions";
  import { sendCmd } from "../api/websocket";

  const weekDays = ["So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"];
  const zoneColors = [
    "#a855f7",
    "#22d3ee",
    "#22c55e",
    "#f59e0b",
    "#f97316",
    "#38bdf8",
  ];

  let info = "";
  let error = "";
  let busy = false;
  let backendOnline = false;
  let selectedZoneId: string | null = null;
  let zoneSettingsExpanded = true;
  let sidebarCollapsed = false;
  let lastZoneSignature = "";

  function normalizePoints(
    points: Array<[number, number]> | Point[] | undefined,
  ): Point[] {
    if (!points) return [];
    return points.map((point) =>
      Array.isArray(point) ? { x: point[0], y: point[1] } : point,
    );
  }

  function normalizeZone(zone: MapZone, index: number): Zone {
    return {
      id: zone.id,
      order: zone.order ?? index + 1,
      polygon: normalizePoints(zone.polygon),
      settings: {
        name: zone.settings.name ?? `Zone ${index + 1}`,
        stripWidth: zone.settings.stripWidth ?? 0.18,
        angle: zone.settings.angle ?? 0,
        edgeMowing: zone.settings.edgeMowing ?? true,
        edgeRounds: zone.settings.edgeRounds ?? 1,
        speed: zone.settings.speed ?? 1.0,
        pattern: zone.settings.pattern ?? "stripe",
      },
    };
  }

  type Segment = { a: Point; b: Point };

  function orientation(a: Point, b: Point, c: Point) {
    return (b.y - a.y) * (c.x - b.x) - (b.x - a.x) * (c.y - b.y);
  }

  function onSegment(a: Point, b: Point, c: Point) {
    return (
      Math.min(a.x, c.x) <= b.x &&
      b.x <= Math.max(a.x, c.x) &&
      Math.min(a.y, c.y) <= b.y &&
      b.y <= Math.max(a.y, c.y)
    );
  }

  function segmentsIntersect(s1: Segment, s2: Segment) {
    const o1 = orientation(s1.a, s1.b, s2.a);
    const o2 = orientation(s1.a, s1.b, s2.b);
    const o3 = orientation(s2.a, s2.b, s1.a);
    const o4 = orientation(s2.a, s2.b, s1.b);

    if (o1 === 0 && onSegment(s1.a, s2.a, s1.b)) return true;
    if (o2 === 0 && onSegment(s1.a, s2.b, s1.b)) return true;
    if (o3 === 0 && onSegment(s2.a, s1.a, s2.b)) return true;
    if (o4 === 0 && onSegment(s2.a, s1.b, s2.b)) return true;

    return o1 > 0 !== o2 > 0 && o3 > 0 !== o4 > 0;
  }

  function hasSelfIntersection(points: Point[]) {
    if (points.length < 4) return false;
    const segments: Segment[] = points.map((point, index) => ({
      a: point,
      b: points[(index + 1) % points.length],
    }));

    for (let i = 0; i < segments.length; i += 1) {
      for (let j = i + 1; j < segments.length; j += 1) {
        const adjacent = j === i + 1 || (i === 0 && j === segments.length - 1);
        if (adjacent) continue;
        if (segmentsIntersect(segments[i], segments[j])) return true;
      }
    }
    return false;
  }

  function isZoneValid(zone: Zone) {
    return zone.polygon.length >= 3 && !hasSelfIntersection(zone.polygon);
  }

  function normalizeMission(doc: MissionDocument): Mission {
    return {
      id: doc.id,
      name: doc.name,
      zoneIds: doc.zoneIds ?? [],
      overrides: doc.overrides ?? {},
      schedule: doc.schedule,
    };
  }

  async function ensureZonesLoaded() {
    if ($mapStore.map.zones.length > 0 || $mapStore.dirty) return;
    busy = true;
    error = "";
    try {
      const map = await getMapDocument();
      mapStore.load({
        perimeter: normalizePoints(map.perimeter),
        dock: normalizePoints(map.dock),
        mow: normalizePoints(map.mow),
        exclusions: (map.exclusions ?? []).map((exclusion) =>
          normalizePoints(exclusion as Array<[number, number]>),
        ),
        zones: (map.zones ?? []).map((zone, index) =>
          normalizeZone(zone, index),
        ),
      });
      info = "Zonen aus Karte geladen";
    } catch (err) {
      error =
        err instanceof Error
          ? err.message
          : "Zonen konnten nicht geladen werden";
    } finally {
      busy = false;
    }
  }

  async function loadMissionsFromBackend() {
    try {
      const missions = await getMissions();
      missionStore.replaceAll(
        missions.map((mission) => normalizeMission(mission)),
        $mapStore.map.zones,
      );
      backendOnline = true;
      if (missions.length > 0) info = "Missionen vom Server geladen";
    } catch {
      backendOnline = false;
      info = "Missionen lokal geladen";
    }
  }

  async function saveCurrentMission() {
    if (!selectedMission) return;
    try {
      await updateMissionDocument(selectedMission.id, selectedMission);
      backendOnline = true;
      info = `Mission "${selectedMission.name}" gespeichert`;
      error = "";
    } catch (err) {
      backendOnline = false;
      error =
        err instanceof Error
          ? err.message
          : "Mission konnte nicht gespeichert werden";
    }
  }

  async function createMission() {
    const missionId = missionStore.createMission($mapStore.map.zones);
    const created = get(missionStore).missions.find(
      (mission) => mission.id === missionId,
    );
    if (!created) return;
    if (backendOnline) {
      try {
        await createMissionDocument(created);
        info = "Neue Mission angelegt";
        error = "";
      } catch (err) {
        backendOnline = false;
        error =
          err instanceof Error
            ? err.message
            : "Neue Mission konnte nicht am Server angelegt werden";
      }
    } else {
      info = "Neue Mission lokal angelegt";
    }
  }

  async function deleteMission(missionId: string) {
    missionStore.deleteMission(missionId);
    if (backendOnline) {
      try {
        await deleteMissionDocument(missionId);
        info = "Mission gelöscht";
        error = "";
      } catch (err) {
        backendOnline = false;
        error =
          err instanceof Error
            ? err.message
            : "Mission konnte nicht gelöscht werden";
      }
    } else {
      info = "Mission lokal gelöscht";
    }
  }

  function updateMissionName(missionId: string, event: Event) {
    missionStore.renameMission(
      missionId,
      (event.currentTarget as HTMLInputElement).value,
    );
  }

  function toggleDay(missionId: string, day: number) {
    missionStore.toggleMissionDay(missionId, day);
  }

  function updateScheduleField<
    K extends "startTime" | "endTime" | "rainDelayMinutes" | "enabled",
  >(
    missionId: string,
    key: K,
    value: K extends "enabled"
      ? boolean
      : K extends "rainDelayMinutes"
        ? number
        : string,
  ) {
    missionStore.updateMissionSchedule(missionId, { [key]: value });
  }

  function zoneName(zoneId: string): string {
    return (
      $mapStore.map.zones.find((z) => z.id === zoneId)?.settings.name ?? zoneId
    );
  }

  function zoneColor(zoneId: string): string {
    if (!selectedMission) return zoneColors[0];
    const index = selectedMission.zoneIds.indexOf(zoneId);
    return zoneColors[index % zoneColors.length] ?? zoneColors[0];
  }

  function addZonesFromPicker(event: Event) {
    if (!selectedMission) return;
    const select = event.currentTarget as HTMLSelectElement;
    const selectedIds = Array.from(select.selectedOptions).map((option) => option.value);
    if (selectedIds.length === 0) return;

    const addIds = selectedIds.filter((zoneId) => {
      const zone = $mapStore.map.zones.find((entry) => entry.id === zoneId);
      return !!zone && isZoneValid(zone) && !selectedMission.zoneIds.includes(zoneId);
    });
    if (addIds.length === 0) return;

    missionStore.setMissionZoneIds(
      selectedMission.id,
      [...selectedMission.zoneIds, ...addIds],
      $mapStore.map.zones,
    );
    selectedZoneId = addIds[addIds.length - 1];
    zoneSettingsExpanded = true;

    // Optional convenience: clear current multi-selection after applying.
    for (const option of Array.from(select.options)) option.selected = false;
  }

  function removeZoneFromMission(zoneId: string) {
    if (!selectedMission) return;
    const missionId = selectedMission.id;
    missionStore.setMissionZoneIds(
      missionId,
      selectedMission.zoneIds.filter((entry) => entry !== zoneId),
      $mapStore.map.zones,
    );
    const updatedMission = get(missionStore).missions.find(
      (mission) => mission.id === missionId,
    );
    if (selectedZoneId === zoneId) {
      selectedZoneId = updatedMission?.zoneIds[0] ?? null;
    }
  }

  function startMission(missionId: string) {
    sendCmd("start", { missionId });
  }

  async function saveZoneOverrides() {
    if (!selectedMission) return;
    try {
      await updateMissionDocument(selectedMission.id, selectedMission);
      backendOnline = true;
      info = "Missions-Zonen-Einstellungen gespeichert";
      error = "";
    } catch (err) {
      backendOnline = false;
      error =
        err instanceof Error
          ? err.message
          : "Missions-Zonen-Einstellungen konnten nicht gespeichert werden";
      info = "";
    }
  }

  function onZoneSelect(event: Event) {
    const value = (event.currentTarget as HTMLSelectElement).value;
    selectedZoneId = value || null;
  }

  function scheduleSummary(mission: Mission): string {
    if (!mission.schedule?.days || mission.schedule.days.length === 0)
      return "Manuell";
    const labels = mission.schedule.days.map((d) => weekDays[d]).join(", ");
    return `${labels} · ${mission.schedule.startTime ?? ""}`;
  }

  $: {
    const zoneSignature = $mapStore.map.zones
      .map((z) => `${z.id}:${z.order}`)
      .join("|");
    if (zoneSignature !== lastZoneSignature) {
      lastZoneSignature = zoneSignature;
      missionStore.syncZones($mapStore.map.zones);
      if ($mapStore.map.zones.length > 0) {
        missionStore.ensureSeedMission($mapStore.map.zones);
      }
    }
  }

  $: selectedMission =
    $missionStore.missions.find(
      (m) => m.id === $missionStore.selectedMissionId,
    ) ?? null;

  $: if (
    selectedMission &&
    selectedZoneId &&
    !selectedMission.zoneIds.includes(selectedZoneId)
  ) {
    selectedZoneId = selectedMission.zoneIds[0] ?? null;
  }

  $: if (
    !selectedMission &&
    selectedZoneId &&
    !$mapStore.map.zones.some((zone) => zone.id === selectedZoneId)
  ) {
    selectedZoneId = null;
  }

  $: selectedZone = selectedZoneId
    ? ($mapStore.map.zones.find((z) => z.id === selectedZoneId) ?? null)
    : null;

  $: selectedZoneColor =
    selectedMission && selectedZoneId
      ? (zoneColors[
          selectedMission.zoneIds.indexOf(selectedZoneId) % zoneColors.length
        ] ?? zoneColors[0])
      : zoneColors[0];

  $: selectedMissionZones = selectedMission
    ? selectedMission.zoneIds
        .map((zoneId) => $mapStore.map.zones.find((zone) => zone.id === zoneId))
        .filter((zone): zone is Zone => Boolean(zone))
    : [];

  $: validZones = $mapStore.map.zones.filter((zone) => isZoneValid(zone));
  $: selectableZones = selectedMission
    ? validZones.filter((zone) => !selectedMission.zoneIds.includes(zone.id))
    : $mapStore.map.zones;
  $: unassignedZonesCount = selectableZones.length;

  onMount(async () => {
    await ensureZonesLoaded();
    await loadMissionsFromBackend();
  });
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="ms-body">
    <div class="ms-canvas-wrap">
      <PathPreview
        map={$mapStore.map}
        zones={$mapStore.map.zones}
        perimeter={$mapStore.map.perimeter}
        exclusions={$mapStore.map.exclusions}
        dock={$mapStore.map.dock}
        mission={selectedMission}
        {selectedZoneId}
        on:selectzone={(e) => {
          selectedZoneId = e.detail.zoneId;
        }}
      />
    </div>

    {#if selectedMission}
      <div class="ms-zone-panel">
        <div class="ms-zone-panel-hd">
          <button
            type="button"
            class="ms-zone-toggle"
            on:click={() => (zoneSettingsExpanded = !zoneSettingsExpanded)}
            aria-expanded={zoneSettingsExpanded}
          >
            {zoneSettingsExpanded ? "▾" : "▸"} Zonen-Einstellungen
          </button>
          <select
            class="ms-zone-select"
            value={selectedZoneId ?? ""}
            on:change={onZoneSelect}
            disabled={selectedMissionZones.length === 0}
          >
            <option value="" disabled={selectedMissionZones.length > 0}>
              {selectedMissionZones.length > 0 ? "Zone auswaehlen" : "Keine Zone in dieser Mission"}
            </option>
            {#each selectedMissionZones as zone}
              <option value={zone.id}>{zone.settings.name}</option>
            {/each}
          </select>
        </div>

        {#if zoneSettingsExpanded}
          {#if selectedZone}
            <ZoneSettings
              mission={selectedMission}
              zone={selectedZone}
              color={selectedZoneColor}
              showHeader={false}
              on:save={saveZoneOverrides}
              on:close={() => {
                zoneSettingsExpanded = false;
              }}
            />
          {:else}
            <div class="ms-zone-empty">
              Waehle oben eine Zone aus, um Bahnen und Parameter zu bearbeiten.
            </div>
          {/if}
        {/if}
      </div>
    {/if}
  </div>

  <svelte:fragment slot="bottom">
    <BottomPanel />
  </svelte:fragment>

  <svelte:fragment slot="sidebar">
    <div class="ms-sb-hd">
      <span class="ms-sb-title">Missionen</span>
      <button type="button" class="ms-add-btn" on:click={createMission}
        >+ Neu</button
      >
    </div>

    <!-- Mission list -->
    <div class="ms-sb-list">
      {#if $missionStore.missions.length === 0}
        <div class="ms-empty">
          {busy ? "Lade Zonen…" : "Noch keine Missionen angelegt."}
        </div>
      {:else}
        {#each $missionStore.missions as mission}
          <div
            role="button"
            tabindex="0"
            class="ms-mcard"
            class:sel={mission.id === $missionStore.selectedMissionId}
            on:click={() => missionStore.selectMission(mission.id)}
            on:keydown={(e) => {
              if (e.key === "Enter" || e.key === " ")
                missionStore.selectMission(mission.id);
            }}
          >
            <div class="ms-mc-row">
              <span class="ms-mc-name">{mission.name}</span>
              <button
                type="button"
                class="ms-mc-run"
                on:click|stopPropagation={() => startMission(mission.id)}
                >▶ Start</button
              >
            </div>
            <div class="ms-mc-meta">{mission.zoneIds.length} Zonen</div>
            {#if mission.schedule?.enabled && mission.schedule.days.length > 0}
              <span class="ms-mc-tag tag-s">🕐 {scheduleSummary(mission)}</span>
            {:else}
              <span class="ms-mc-tag tag-m">Manuell</span>
            {/if}
          </div>
        {/each}
      {/if}
    </div>

    <!-- Mission Editor -->
    {#if selectedMission}
      <div class="ms-editor">
        <div class="ms-ed-hd">
          <input
            class="ms-ed-name"
            type="text"
            value={selectedMission.name}
            on:input={(e) => updateMissionName(selectedMission.id, e)}
          />
          <button
            type="button"
            class="ms-save-btn"
            on:click={saveCurrentMission}>Speichern</button
          >
          <button
            type="button"
            class="ms-del-btn"
            on:click={() => deleteMission(selectedMission.id)}>✕</button
          >
        </div>

        <!-- Zones -->
        <div class="ms-ed-sec">
          <div class="ms-ed-slbl">
            <span>Zonen</span>
            <span class="ms-zone-count">{unassignedZonesCount} frei</span>
          </div>
          <div class="ms-zone-picker">
            <select
              class="ms-zone-picker-select"
              multiple
              size="4"
              on:change={addZonesFromPicker}
              disabled={selectableZones.length === 0}
            >
              {#each selectableZones as zone}
                <option value={zone.id}>
                  {zone.settings.name}
                </option>
              {/each}
            </select>
            <div class="ms-zone-picker-hint">Vorhandene Zonen auswählen und zur Mission hinzufügen (Mehrfachauswahl möglich).</div>
          </div>
          {#if selectedMission.zoneIds.length === 0}
            <div class="ms-empty-inline">
              Noch keine Zonen in dieser Mission.
            </div>
          {:else}
            {#each selectedMission.zoneIds as zoneId, index}
              <div
                role="button"
                tabindex="0"
                class="ms-zrow"
                class:sel={selectedZoneId === zoneId}
                on:click={() => {
                  selectedZoneId = zoneId;
                }}
                on:keydown={(e) => {
                  if (e.key === "Enter" || e.key === " ")
                    selectedZoneId = zoneId;
                }}
              >
                <span class="ms-zdrag">⠿</span>
                <span class="ms-znum">{index + 1}</span>
                <span
                  class="ms-zdot"
                  style="background:{zoneColors[index % zoneColors.length]}"
                ></span>
                <span class="ms-zname">{zoneName(zoneId)}</span>
                <button
                  type="button"
                  class="ms-zgear"
                  class:active={selectedZoneId === zoneId}
                  aria-label="Einstellungen für {zoneName(zoneId)}"
                  on:click|stopPropagation={() => {
                    selectedZoneId = selectedZoneId === zoneId ? null : zoneId;
                  }}>⚙</button
                >
                <button
                  type="button"
                  class="ms-zdel"
                  aria-label="{zoneName(zoneId)} aus Mission entfernen"
                  on:click|stopPropagation={() => removeZoneFromMission(zoneId)}
                  >✕</button
                >
              </div>
            {/each}
          {/if}
        </div>

        <!-- Schedule -->
        <div class="ms-ed-sec">
          <div class="ms-ed-slbl">Zeitplan</div>
          <div class="ms-days">
            {#each weekDays as dayLabel, dayIndex}
              <button
                type="button"
                class="ms-day"
                class:on={selectedMission.schedule?.days.includes(dayIndex)}
                on:click={() => toggleDay(selectedMission.id, dayIndex)}
                >{dayLabel}</button
              >
            {/each}
          </div>
          <div class="ms-timeg">
            <div class="ms-tfield">
              <span class="ms-tlbl">Start</span>
              <input
                class="ms-tin"
                type="time"
                value={selectedMission.schedule?.startTime ?? "09:00"}
                on:input={(e) =>
                  updateScheduleField(
                    selectedMission.id,
                    "startTime",
                    (e.currentTarget as HTMLInputElement).value,
                  )}
              />
            </div>
            <div class="ms-tfield">
              <span class="ms-tlbl">Ende</span>
              <input
                class="ms-tin"
                type="time"
                value={selectedMission.schedule?.endTime ?? "12:00"}
                on:input={(e) =>
                  updateScheduleField(
                    selectedMission.id,
                    "endTime",
                    (e.currentTarget as HTMLInputElement).value,
                  )}
              />
            </div>
            <div class="ms-tfield">
              <span class="ms-tlbl">Regenpause (min)</span>
              <input
                class="ms-tin"
                type="number"
                min="0"
                step="5"
                value={selectedMission.schedule?.rainDelayMinutes ?? 60}
                on:input={(e) =>
                  updateScheduleField(
                    selectedMission.id,
                    "rainDelayMinutes",
                    Number((e.currentTarget as HTMLInputElement).value),
                  )}
              />
            </div>
            <div class="ms-tfield">
              <label class="ms-toggle-row">
                <input
                  type="checkbox"
                  checked={selectedMission.schedule?.enabled ?? false}
                  on:change={(e) =>
                    updateScheduleField(
                      selectedMission.id,
                      "enabled",
                      (e.currentTarget as HTMLInputElement).checked,
                    )}
                />
                <span class="ms-tlbl" style="margin-left:5px">Aktiv</span>
              </label>
            </div>
          </div>
        </div>
      </div>
    {/if}

    <!-- Status notifications -->
    {#if error}
      <div class="ms-notify ms-notify-err">{error}</div>
    {:else if info}
      <div class="ms-notify ms-notify-ok">{info}</div>
    {/if}
  </svelte:fragment>
</PageLayout>

<style>
  /* Center column: preview + settings panel */
  .ms-body {
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
  }

  .ms-canvas-wrap {
    flex: 1;
    position: relative;
    background: #070d18;
    overflow: hidden;
  }

  .ms-zone-panel {
    flex-shrink: 0;
    border-top: 1px solid #1e3a5f;
    background: #0a1020;
  }

  .ms-zone-panel-hd {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 8px 12px;
    border-bottom: 1px solid #1e3a5f;
  }

  .ms-zone-toggle {
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #cbd5e1;
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    cursor: pointer;
  }

  .ms-zone-select {
    min-width: 220px;
    max-width: 340px;
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #e2e8f0;
    border-radius: 6px;
    padding: 4px 8px;
    font-size: 12px;
  }

  .ms-zone-select:disabled {
    opacity: 0.55;
    cursor: not-allowed;
  }

  .ms-zone-empty {
    padding: 10px 12px;
    font-size: 11px;
    color: #64748b;
  }

  .ms-sb-hd {
    padding: 10px 14px;
    border-bottom: 1px solid #1e3a5f;
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-shrink: 0;
  }

  .ms-sb-title {
    font-size: 12px;
    font-weight: 500;
    color: #e2e8f0;
  }

  .ms-add-btn {
    background: #0c1a3a;
    border: 1px solid #2563eb;
    color: #93c5fd;
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    cursor: pointer;
  }

  .ms-add-btn:hover {
    border-color: #3b82f6;
  }

  .ms-add-btn:disabled {
    opacity: 0.45;
    cursor: not-allowed;
    border-color: #1e3a5f;
    color: #64748b;
  }

  /* Mission cards */
  .ms-sb-list {
    padding: 8px;
    display: flex;
    flex-direction: column;
    gap: 5px;
    border-bottom: 1px solid #1e3a5f;
    flex-shrink: 0;
  }

  .ms-mcard {
    background: #0f1829;
    border: 1px solid #1a2a40;
    border-radius: 7px;
    padding: 8px 10px;
    cursor: pointer;
  }

  .ms-mcard:hover {
    border-color: #1e3a5f;
  }
  .ms-mcard.sel {
    border-color: #2563eb;
    background: #0c1a3a;
  }

  .ms-mc-row {
    display: flex;
    align-items: center;
    gap: 6px;
  }

  .ms-mc-name {
    font-size: 12px;
    font-weight: 500;
    color: #e2e8f0;
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .ms-mc-run {
    background: #0c1a3a;
    border: 1px solid #2563eb;
    color: #93c5fd;
    border-radius: 5px;
    padding: 2px 8px;
    font-size: 11px;
    cursor: pointer;
    flex-shrink: 0;
    white-space: nowrap;
  }

  .ms-mc-run:hover {
    background: #0f2040;
  }

  .ms-mc-meta {
    font-size: 10px;
    color: #64748b;
    margin-top: 2px;
  }

  .ms-mc-tag {
    display: inline-flex;
    font-size: 10px;
    padding: 1px 6px;
    border-radius: 8px;
    margin-top: 3px;
  }

  .tag-s {
    background: #0c1a3a;
    border: 1px solid #1e40af;
    color: #93c5fd;
  }
  .tag-m {
    background: #111827;
    border: 1px solid #374151;
    color: #6b7280;
  }

  /* Mission editor */
  .ms-editor {
    flex-shrink: 0;
    display: flex;
    flex-direction: column;
  }

  .ms-ed-hd {
    padding: 9px 12px;
    border-bottom: 1px solid #1e3a5f;
    display: flex;
    align-items: center;
    gap: 6px;
    flex-shrink: 0;
  }

  .ms-ed-name {
    flex: 1;
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #e2e8f0;
    border-radius: 6px;
    padding: 5px 9px;
    font-size: 13px;
    font-weight: 500;
    min-width: 0;
  }

  .ms-ed-name:focus {
    outline: none;
    border-color: #2563eb;
  }

  .ms-save-btn {
    background: #0c1a3a;
    border: 1px solid #2563eb;
    color: #93c5fd;
    border-radius: 5px;
    padding: 5px 10px;
    font-size: 11px;
    cursor: pointer;
    flex-shrink: 0;
  }

  .ms-del-btn {
    background: #1a0808;
    border: 1px solid #7f1d1d;
    color: #f87171;
    border-radius: 5px;
    padding: 5px 8px;
    font-size: 11px;
    cursor: pointer;
    flex-shrink: 0;
  }

  .ms-ed-sec {
    padding: 7px 12px;
    border-bottom: 1px solid #1a2a40;
  }

  .ms-ed-sec:last-child {
    border-bottom: none;
  }

  .ms-ed-slbl {
    font-size: 10px;
    font-weight: 500;
    color: #475569;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 6px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  .ms-zone-count {
    font-size: 10px;
    color: #64748b;
    letter-spacing: 0;
    text-transform: none;
    font-weight: 400;
  }

  .ms-zone-picker {
    margin-bottom: 6px;
  }

  .ms-zone-picker-select {
    width: 100%;
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #e2e8f0;
    border-radius: 5px;
    padding: 5px 8px;
    font-size: 11px;
  }

  .ms-zone-picker-select:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .ms-zone-picker-hint {
    margin-top: 4px;
    font-size: 10px;
    color: #64748b;
  }

  /* Zone rows */
  .ms-zrow {
    display: flex;
    align-items: center;
    gap: 5px;
    padding: 4px 0;
    border-bottom: 1px solid #0a1020;
    overflow: hidden;
    min-width: 0;
    cursor: pointer;
  }

  .ms-zrow:last-child {
    border-bottom: none;
  }

  .ms-zrow.sel {
    background: #0c1a3a;
    border-radius: 5px;
    padding: 4px 4px;
    margin: 0 -4px;
    border-bottom: none;
  }

  .ms-zdrag {
    color: #475569;
    font-size: 12px;
    cursor: grab;
    flex-shrink: 0;
  }
  .ms-znum {
    font-size: 10px;
    color: #475569;
    width: 14px;
    text-align: center;
    flex-shrink: 0;
  }

  .ms-zdot {
    width: 9px;
    height: 9px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .ms-zname {
    font-size: 12px;
    color: #e2e8f0;
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .ms-zgear {
    background: transparent;
    border: 1px solid #1e3a5f;
    color: #60a5fa;
    border-radius: 4px;
    padding: 2px 5px;
    font-size: 11px;
    cursor: pointer;
    flex-shrink: 0;
  }

  .ms-zgear.active {
    background: #0c1a3a;
    border-color: #2563eb;
  }

  .ms-zdel {
    background: transparent;
    border: none;
    color: #475569;
    cursor: pointer;
    font-size: 12px;
    flex-shrink: 0;
    padding: 0 2px;
  }

  .ms-zdel:hover {
    color: #f87171;
  }

  /* Schedule */
  .ms-days {
    display: flex;
    gap: 3px;
    margin-bottom: 7px;
  }

  .ms-day {
    flex: 1;
    height: 26px;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 5px;
    font-size: 11px;
    font-weight: 500;
    cursor: pointer;
    border: 1px solid #1e3a5f;
    background: #0a1020;
    color: #64748b;
  }

  .ms-day.on {
    background: #0c1a3a;
    border-color: #2563eb;
    color: #93c5fd;
  }

  .ms-timeg {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 6px;
  }

  .ms-tfield {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }

  .ms-tlbl {
    font-size: 10px;
    color: #475569;
  }

  .ms-tin {
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #e2e8f0;
    border-radius: 5px;
    padding: 4px 6px;
    font-size: 11px;
    width: 100%;
  }

  .ms-tin:focus {
    outline: none;
    border-color: #2563eb;
  }

  .ms-toggle-row {
    display: flex;
    align-items: center;
    cursor: pointer;
    padding-top: 10px;
  }

  /* Status notifications */
  .ms-notify {
    margin: 8px;
    padding: 6px 10px;
    border-radius: 6px;
    font-size: 11px;
    flex-shrink: 0;
  }

  .ms-notify-ok {
    background: #0a1e0f;
    border: 1px solid #166534;
    color: #4ade80;
  }
  .ms-notify-err {
    background: #1a0808;
    border: 1px solid #7f1d1d;
    color: #fca5a5;
  }

  .ms-empty {
    font-size: 11px;
    color: #475569;
    padding: 4px 2px;
  }

  .ms-empty-inline {
    font-size: 11px;
    color: #475569;
    padding: 4px 0;
  }

</style>
