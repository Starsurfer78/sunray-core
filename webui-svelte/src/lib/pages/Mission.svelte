<script lang="ts">
  import { get } from 'svelte/store'
  import { onMount } from 'svelte'
  import PathPreview from '../components/Mission/PathPreview.svelte'
  import ZoneSettings from '../components/Mission/ZoneSettings.svelte'
  import MissionControls from '../components/Mission/MissionControls.svelte'
  import {
    createMissionDocument,
    deleteMissionDocument,
    getMapDocument,
    getMissions,
    updateMissionDocument,
    type MapZone,
    type MissionDocument,
  } from '../api/rest'
  import { mapStore, type Point, type Zone } from '../stores/map'
  import { missionStore, type Mission } from '../stores/missions'

  const weekDays = ['So', 'Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa']

  let info = ''
  let error = ''
  let busy = false
  let backendOnline = false
  let selectedZoneId: string | null = null
  let lastZoneSignature = ''

  function normalizePoints(points: Array<[number, number]> | Point[] | undefined): Point[] {
    if (!points) return []
    return points.map((point) =>
      Array.isArray(point)
        ? { x: point[0], y: point[1] }
        : point,
    )
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
        pattern: zone.settings.pattern ?? 'stripe',
      },
    }
  }

  function normalizeMission(doc: MissionDocument): Mission {
    return {
      id: doc.id,
      name: doc.name,
      zoneIds: doc.zoneIds ?? [],
      overrides: doc.overrides ?? {},
      schedule: doc.schedule,
    }
  }

  async function ensureZonesLoaded() {
    if ($mapStore.map.zones.length > 0) return

    busy = true
    error = ''
    try {
      const map = await getMapDocument()
      mapStore.load({
        perimeter: normalizePoints(map.perimeter),
        dock: normalizePoints(map.dock),
        mow: normalizePoints(map.mow),
        exclusions: (map.exclusions ?? []).map((exclusion) => normalizePoints(exclusion as Array<[number, number]>)),
        zones: (map.zones ?? []).map((zone, index) => normalizeZone(zone, index)),
      })
      info = 'Zonen aus Karte geladen'
    } catch (err) {
      error = err instanceof Error ? err.message : 'Zonen konnten nicht geladen werden'
    } finally {
      busy = false
    }
  }

  async function loadMissionsFromBackend() {
    try {
      const missions = await getMissions()
      missionStore.replaceAll(missions.map((mission) => normalizeMission(mission)), $mapStore.map.zones)
      backendOnline = true
      if (missions.length > 0) info = 'Missionen vom Server geladen'
    } catch {
      backendOnline = false
      info = 'Missionen lokal geladen'
    }
  }

  async function saveCurrentMission() {
    if (!selectedMission) return
    try {
      await updateMissionDocument(selectedMission.id, selectedMission)
      backendOnline = true
      info = `Mission "${selectedMission.name}" gespeichert`
      error = ''
    } catch (err) {
      backendOnline = false
      error = err instanceof Error ? err.message : 'Mission konnte nicht gespeichert werden'
    }
  }

  async function createMission() {
    const missionId = missionStore.createMission($mapStore.map.zones)
    const created = get(missionStore).missions.find((mission) => mission.id === missionId)
    if (!created) return
    if (backendOnline) {
      try {
        await createMissionDocument(created)
        info = 'Neue Mission angelegt'
        error = ''
      } catch (err) {
        backendOnline = false
        error = err instanceof Error ? err.message : 'Neue Mission konnte nicht am Server angelegt werden'
      }
    } else {
      info = 'Neue Mission lokal angelegt'
    }
  }

  async function deleteMission(missionId: string) {
    missionStore.deleteMission(missionId)
    if (backendOnline) {
      try {
        await deleteMissionDocument(missionId)
        info = 'Mission geloescht'
        error = ''
      } catch (err) {
        backendOnline = false
        error = err instanceof Error ? err.message : 'Mission konnte nicht geloescht werden'
      }
    } else {
      info = 'Mission lokal geloescht'
    }
  }

  function updateMissionName(missionId: string, event: Event) {
    missionStore.renameMission(missionId, (event.currentTarget as HTMLInputElement).value)
  }

  function toggleDay(missionId: string, day: number) {
    missionStore.toggleMissionDay(missionId, day)
  }

  function updateScheduleField<K extends 'startTime' | 'endTime' | 'rainDelayMinutes' | 'enabled'>(
    missionId: string,
    key: K,
    value: K extends 'enabled' ? boolean : K extends 'rainDelayMinutes' ? number : string,
  ) {
    missionStore.updateMissionSchedule(missionId, { [key]: value })
  }

  function zoneName(zoneId: string): string {
    return $mapStore.map.zones.find((zone) => zone.id === zoneId)?.settings.name ?? zoneId
  }

  function addZoneToMission() {
    if (!selectedMission) return
    const nextZone = $mapStore.map.zones.find((zone) => !selectedMission.zoneIds.includes(zone.id))
    if (!nextZone) return
    missionStore.setMissionZoneIds(selectedMission.id, [...selectedMission.zoneIds, nextZone.id], $mapStore.map.zones)
    selectedZoneId = nextZone.id
  }

  function removeZoneFromMission(zoneId: string) {
    if (!selectedMission) return
    missionStore.setMissionZoneIds(
      selectedMission.id,
      selectedMission.zoneIds.filter((entry) => entry !== zoneId),
      $mapStore.map.zones,
    )
    if (selectedZoneId === zoneId) selectedZoneId = selectedMission.zoneIds.find((entry) => entry !== zoneId) ?? null
  }

  $: {
    const zoneSignature = $mapStore.map.zones.map((zone) => `${zone.id}:${zone.order}`).join('|')
    if (zoneSignature !== lastZoneSignature) {
      lastZoneSignature = zoneSignature
      missionStore.syncZones($mapStore.map.zones)
      if ($mapStore.map.zones.length > 0) {
        missionStore.ensureSeedMission($mapStore.map.zones)
      }
    }
  }

  $: selectedMission =
    $missionStore.missions.find((mission) => mission.id === $missionStore.selectedMissionId) ?? null

  $: if (selectedMission && (!selectedZoneId || !selectedMission.zoneIds.includes(selectedZoneId))) {
    selectedZoneId = selectedMission.zoneIds[0] ?? null
  }

  $: selectedZone =
    selectedZoneId
      ? $mapStore.map.zones.find((zone) => zone.id === selectedZoneId) ?? null
      : null

  onMount(async () => {
    await ensureZonesLoaded()
    await loadMissionsFromBackend()
  })
</script>

<main class="page">
  <section class="head">
    <div>
      <span class="eyebrow">Missionen</span>
      <h1>Missionen planen, Zonen gruppieren und Abläufe vorbereiten</h1>
    </div>
    <div class="status-card">
      <span class="label">Status</span>
      <strong>{busy ? 'Lade Zonen' : 'Bereit'}</strong>
      <span class={backendOnline ? 'ok' : 'muted'}>
        {backendOnline ? 'Backend-Speicherung aktiv' : 'Nur lokale Browser-Speicherung aktiv'}
      </span>
      {#if error}
        <span class="error">{error}</span>
      {:else if info}
        <span class="ok">{info}</span>
      {/if}
    </div>
  </section>

  <section class="layout">
    <div class="workspace">
      <PathPreview
        zones={$mapStore.map.zones}
        mission={selectedMission}
        {selectedZoneId}
        on:selectzone={(event) => { selectedZoneId = event.detail.zoneId }}
      />

      {#if selectedMission && selectedZone}
        <ZoneSettings mission={selectedMission} zone={selectedZone} />
      {/if}
    </div>

    <aside class="sidebar">
      <div class="sidebar-head">
        <span class="sidebar-title">Missionen</span>
        <button type="button" class="create-btn" on:click={createMission}>+ Neu</button>
      </div>

      <div class="mission-list">
        {#if $missionStore.missions.length === 0}
          <div class="empty-list">Noch keine Missionen angelegt.</div>
        {:else}
          {#each $missionStore.missions as mission}
            <button
              type="button"
              class:selected={mission.id === $missionStore.selectedMissionId}
              class="mission-card"
              on:click={() => missionStore.selectMission(mission.id)}
            >
              <div class="mission-row">
                <strong>{mission.name}</strong>
                <span>{mission.zoneIds.length} Zonen</span>
              </div>
              <small>
                {#if mission.schedule?.enabled && mission.schedule.days.length > 0}
                  {mission.schedule.days.map((day) => weekDays[day]).join(', ')} · {mission.schedule.startTime}
                {:else}
                  Manuell
                {/if}
              </small>
            </button>
          {/each}
        {/if}
      </div>

      {#if selectedMission}
        <div class="editor">
          <div class="editor-head">
            <input
              class="mission-name"
              type="text"
              value={selectedMission.name}
              on:input={(event) => updateMissionName(selectedMission.id, event)}
            />
            <button type="button" class="save-btn" on:click={saveCurrentMission}>Speichern</button>
            <button type="button" class="delete-btn" on:click={() => deleteMission(selectedMission.id)}>✕</button>
          </div>

          <div class="editor-section">
            <div class="section-head">
              <span class="section-label">Zonen</span>
              <button type="button" class="inline-btn" on:click={addZoneToMission}>+ hinzufügen</button>
            </div>

            <div class="zone-rows">
              {#if selectedMission.zoneIds.length === 0}
                <div class="empty-inline">Noch keine Zonen in dieser Mission.</div>
              {:else}
                {#each selectedMission.zoneIds as zoneId, index}
                  <div
                    role="button"
                    tabindex="0"
                    class:selected={selectedZoneId === zoneId}
                    class="zone-row"
                    on:click={() => { selectedZoneId = zoneId }}
                    on:keydown={(event) => {
                      if (event.key === 'Enter' || event.key === ' ') {
                        event.preventDefault()
                        selectedZoneId = zoneId
                      }
                    }}
                  >
                    <span class="zone-order">{index + 1}</span>
                    <span class="zone-title">{zoneName(zoneId)}</span>
                    <span class="zone-gear">⚙</span>
                    <button
                      type="button"
                      class="zone-delete"
                      aria-label={`${zoneName(zoneId)} aus Mission entfernen`}
                      on:click|stopPropagation={() => removeZoneFromMission(zoneId)}
                    >✕</button>
                  </div>
                {/each}
              {/if}
            </div>
          </div>

          <div class="editor-section">
            <div class="section-label">Zeitplan</div>
            <div class="days">
              {#each weekDays as dayLabel, dayIndex}
                <button
                  type="button"
                  class:active={selectedMission.schedule?.days.includes(dayIndex)}
                  class="day-btn"
                  on:click={() => toggleDay(selectedMission.id, dayIndex)}
                >
                  {dayLabel}
                </button>
              {/each}
            </div>

            <div class="schedule-grid">
              <label>
                <span>Start</span>
                <input
                  type="time"
                  value={selectedMission.schedule?.startTime ?? '09:00'}
                  on:input={(event) => updateScheduleField(selectedMission.id, 'startTime', (event.currentTarget as HTMLInputElement).value)}
                />
              </label>
              <label>
                <span>Ende</span>
                <input
                  type="time"
                  value={selectedMission.schedule?.endTime ?? '12:00'}
                  on:input={(event) => updateScheduleField(selectedMission.id, 'endTime', (event.currentTarget as HTMLInputElement).value)}
                />
              </label>
              <label>
                <span>Regenpause</span>
                <input
                  type="number"
                  min="0"
                  step="5"
                  value={selectedMission.schedule?.rainDelayMinutes ?? 60}
                  on:input={(event) => updateScheduleField(selectedMission.id, 'rainDelayMinutes', Number((event.currentTarget as HTMLInputElement).value))}
                />
              </label>
              <label class="toggle-field">
                <span>Aktiv</span>
                <input
                  type="checkbox"
                  checked={selectedMission.schedule?.enabled ?? false}
                  on:change={(event) => updateScheduleField(selectedMission.id, 'enabled', (event.currentTarget as HTMLInputElement).checked)}
                />
              </label>
            </div>
          </div>

          <MissionControls zones={$mapStore.map.zones} mission={selectedMission} selectedZoneIds={selectedMission.zoneIds} />
        </div>
      {/if}
    </aside>
  </section>
</main>

<style>
  .page {
    display: grid;
    gap: 1rem;
    height: 100%;
    padding: 1rem;
  }
  .head {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 320px;
    gap: 1rem;
    align-items: start;
  }
  .eyebrow {
    display: inline-block;
    margin-bottom: 0.35rem;
    color: #60a5fa;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
  }
  h1 { margin: 0; font-size: 1.55rem; line-height: 1.05; }
  .status-card, .editor {
    display: grid;
    gap: 0.45rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  .label, .section-label, .sidebar-title {
    color: #7a8da8;
    font-size: 0.76rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }
  .muted { color: #94a3b8; font-size: 0.82rem; }
  .ok { color: #a8e2a1; font-size: 0.82rem; }
  .error { color: #f1aaaa; font-size: 0.82rem; }
  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 320px;
    gap: 1rem;
    min-height: 0;
  }
  .workspace {
    display: grid;
    gap: 1rem;
    align-content: start;
    min-width: 0;
  }
  .sidebar {
    display: grid;
    grid-template-rows: auto auto 1fr;
    gap: 1rem;
    min-height: 0;
  }
  .sidebar-head, .mission-row, .editor-head, .section-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.75rem;
  }
  .create-btn, .save-btn, .delete-btn, .day-btn, .inline-btn {
    padding: 0.5rem 0.8rem;
    border: 1px solid #2563eb;
    border-radius: 0.6rem;
    background: #0c1a3a;
    color: #93c5fd;
    font-weight: 700;
    cursor: pointer;
  }
  .inline-btn {
    padding: 0.3rem 0.55rem;
    font-size: 0.74rem;
  }
  .delete-btn {
    border-color: #7f1d1d;
    background: #1a0808;
    color: #fca5a5;
  }
  .mission-list {
    display: grid;
    gap: 0.6rem;
    max-height: 260px;
    overflow: auto;
  }
  .mission-card {
    display: grid;
    gap: 0.35rem;
    padding: 0.8rem 0.9rem;
    border-radius: 0.7rem;
    background: #0f1829;
    border: 1px solid #1a2a40;
    color: #dce8e8;
    text-align: left;
    cursor: pointer;
  }
  .mission-card.selected {
    border-color: #2563eb;
    background: #0c1a3a;
  }
  .mission-card small, .empty-list, .empty-inline {
    color: #64748b;
  }
  .editor {
    min-height: 0;
    overflow: auto;
    align-content: start;
  }
  .mission-name, .schedule-grid input {
    width: 100%;
    padding: 0.5rem 0.7rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.6rem;
    background: #0a1020;
    color: #e2e8f0;
  }
  .editor-section, .zone-rows {
    display: grid;
    gap: 0.6rem;
  }
  .zone-row {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    width: 100%;
    padding: 0.55rem 0.65rem;
    border-radius: 0.6rem;
    background: #0a1020;
    border: 1px solid #1a2a40;
    color: #dce8e8;
    cursor: pointer;
  }
  .zone-row.selected {
    border-color: #2563eb;
    background: #0c1a3a;
  }
  .zone-order {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 1.35rem;
    height: 1.35rem;
    border-radius: 999px;
    background: #1e3a5f;
    color: #93c5fd;
    font-size: 0.75rem;
    font-weight: 700;
    flex-shrink: 0;
  }
  .zone-title {
    flex: 1;
    text-align: left;
  }
  .zone-gear {
    color: #60a5fa;
  }
  .zone-delete {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 1.6rem;
    height: 1.6rem;
    padding: 0;
    border-radius: 999px;
    border: 1px solid #7f1d1d;
    background: #1a0808;
    color: #fca5a5;
    cursor: pointer;
  }
  .days {
    display: grid;
    grid-template-columns: repeat(7, minmax(0, 1fr));
    gap: 0.35rem;
  }
  .day-btn {
    padding: 0.45rem 0;
    background: #0a1020;
    border-color: #1e3a5f;
    color: #64748b;
  }
  .day-btn.active {
    background: #0c1a3a;
    border-color: #2563eb;
    color: #93c5fd;
  }
  .schedule-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.65rem;
  }
  .schedule-grid label {
    display: grid;
    gap: 0.3rem;
    color: #94a3b8;
    font-size: 0.76rem;
  }
  .toggle-field {
    align-items: start;
  }
  .toggle-field input {
    width: auto;
    margin-top: 0.45rem;
  }
  @media (max-width: 1100px) {
    .head, .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
