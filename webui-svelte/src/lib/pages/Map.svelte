<script lang="ts">
  import { onMount } from 'svelte'
  import MapCanvas from '../components/Map/MapCanvas.svelte'
  import { getMapDocument, saveMapDocument } from '../api/rest'
  import { mapStore, type Point, type Zone } from '../stores/map'

  let busy = false
  let info = ''
  let infoTimer: ReturnType<typeof setTimeout> | null = null
  let mapCanvas: MapCanvas
  let sidebarCollapsed = false

  function showInfo(msg: string) {
    info = msg
    if (infoTimer) clearTimeout(infoTimer)
    infoTimer = setTimeout(() => { info = '' }, 2500)
  }

  function normalizePoints(points: Array<[number, number]> | Point[] | undefined): Point[] {
    if (!points) return []
    return points.map((p) => Array.isArray(p) ? { x: p[0], y: p[1] } : p)
  }

  async function loadMap() {
    busy = true
    try {
      const map = await getMapDocument()
      mapStore.load({
        perimeter: normalizePoints(map.perimeter),
        dock: normalizePoints(map.dock),
        mow: normalizePoints(map.mow),
        exclusions: (map.exclusions ?? []).map((e) => normalizePoints(e as Array<[number, number]>)),
        zones: (map.zones ?? []).map((zone: Zone) => ({
          ...zone, polygon: normalizePoints(zone.polygon),
        })),
      })
      showInfo('Geladen')
    } catch (err) {
      showInfo(err instanceof Error ? err.message : 'Fehler')
    } finally { busy = false }
  }

  async function saveMap() {
    busy = true
    try {
      const payload = {
        perimeter: $mapStore.map.perimeter.map((p) => [p.x, p.y]),
        dock: $mapStore.map.dock.map((p) => [p.x, p.y]),
        mow: $mapStore.map.mow.map((p) => [p.x, p.y]),
        exclusions: $mapStore.map.exclusions.map((ex) => ex.map((p) => [p.x, p.y])),
        zones: $mapStore.map.zones.map((zone) => ({
          ...zone, polygon: zone.polygon.map((p) => [p.x, p.y]),
        })),
      }
      const result = await saveMapDocument(payload)
      if (!result.ok) { showInfo(result.error ?? 'Fehler'); return }
      mapStore.markSaved()
      showInfo('Gespeichert')
    } catch (err) {
      showInfo(err instanceof Error ? err.message : 'Fehler')
    } finally { busy = false }
  }

  function exportMap() {
    const data = JSON.stringify({
      perimeter: $mapStore.map.perimeter,
      dock: $mapStore.map.dock,
      exclusions: $mapStore.map.exclusions,
      zones: $mapStore.map.zones,
    }, null, 2)
    const blob = new Blob([data], { type: 'application/json' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = 'map.json'
    a.click()
    URL.revokeObjectURL(url)
    showInfo('Exportiert')
  }

  function importMap() {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json'
    input.onchange = async () => {
      const file = input.files?.[0]
      if (!file) return
      try {
        const text = await file.text()
        const data = JSON.parse(text)
        mapStore.load({
          perimeter: normalizePoints(data.perimeter),
          dock: normalizePoints(data.dock),
          mow: normalizePoints(data.mow ?? []),
          exclusions: (data.exclusions ?? []).map((e: Array<[number, number]>) => normalizePoints(e)),
          zones: (data.zones ?? []).map((zone: Zone) => ({
            ...zone, polygon: normalizePoints(zone.polygon),
          })),
        })
        showInfo('Importiert')
      } catch {
        showInfo('Import fehlgeschlagen')
      }
    }
    input.click()
  }

  $: activeTool = $mapStore.selectedTool
  $: dockActive = activeTool === 'dock'
  $: moveActive = activeTool === 'move'
  $: layer = (activeTool === 'zone' || activeTool === 'nogo') ? activeTool : 'perimeter'

  function setLayer(l: 'perimeter' | 'zone' | 'nogo') {
    if (l === 'zone' && $mapStore.map.zones.length === 0) mapStore.createZone()
    else if (l === 'nogo' && $mapStore.map.exclusions.length === 0) mapStore.createExclusion()
    else mapStore.setTool(l)
  }

  function toggleDock() { mapStore.setTool(dockActive ? layer : 'dock') }
  function toggleMove() { mapStore.setTool(moveActive ? layer : 'move') }

  function addNew() {
    if (activeTool === 'zone') mapStore.createZone()
    else if (activeTool === 'nogo') mapStore.createExclusion()
  }

  function deleteActive() {
    mapStore.clearActive()
  }

  function removeLastPoint() {
    if (activeTool === 'move') mapStore.setTool(layer)
    mapStore.removeLastPoint()
  }

  $: addBtnTitle = dockActive
    ? 'Dockpunkt setzen (Klick ins Raster)'
    : activeTool === 'zone'
      ? 'Zonenpunkt setzen (Klick ins Raster)'
      : activeTool === 'nogo'
        ? 'NoGo-Punkt setzen (Klick ins Raster)'
        : 'Perimeterpunkt setzen (Klick ins Raster)'

  $: hasZones = $mapStore.map.zones.length > 0
  $: hasNogo  = $mapStore.map.exclusions.length > 0

  onMount(() => { void loadMap() })
</script>

<main class="page">
  <div class="map-stage">
    <MapCanvas bind:this={mapCanvas} showHeader={false} showViewportActions={false} interactive={true} />

    <!-- Bottom center: edit tools -->
    <div class="toolbar-wrap">

      <!-- Layer selector -->
      <div class="layer-bar">
        <button type="button" class:active={layer === 'perimeter' && !dockActive} on:click={() => setLayer('perimeter')}>
          <span class="layer-dot perimeter"></span> Perimeter
        </button>
        <button type="button" class:active={layer === 'zone' && !dockActive}
          on:click={() => setLayer('zone')}>
          <span class="layer-dot zone"></span>
          Zone {#if hasZones}<small>({$mapStore.map.zones.length})</small>{/if}
        </button>
        <button type="button" class:active={layer === 'nogo' && !dockActive}
          on:click={() => setLayer('nogo')}>
          <span class="layer-dot nogo"></span>
          NoGo {#if hasNogo}<small>({$mapStore.map.exclusions.length})</small>{/if}
        </button>
        <div class="divider"></div>
        <button type="button" class:active={dockActive} title="Dock-Modus" on:click={toggleDock}>D</button>
        <button type="button" class:active={moveActive} title="Punkte verschieben" on:click={toggleMove}>⇔</button>
        {#if info}
          <div class="divider"></div>
          <span class="info">{info}</span>
        {/if}
      </div>

      <!-- Zone/NoGo: new item row -->
      {#if layer === 'zone' || layer === 'nogo'}
        <div class="item-bar">
          {#if layer === 'zone'}
            {#each $mapStore.map.zones as zone}
              <button type="button"
                class="item-btn"
                class:active={$mapStore.selectedZoneId === zone.id}
                on:click={() => { mapStore.selectZone(zone.id); mapStore.setTool('zone') }}
              >{zone.settings.name}</button>
            {/each}
          {:else}
            {#each $mapStore.map.exclusions as _, i}
              <button type="button"
                class="item-btn nogo"
                class:active={$mapStore.selectedExclusionIndex === i}
                on:click={() => { mapStore.selectExclusion(i); mapStore.setTool('nogo') }}
              >NoGo {i + 1}</button>
            {/each}
          {/if}
          <button type="button" class="item-new" on:click={addNew}>+ Neu</button>
        </div>
      {/if}

      <!-- Big action buttons -->
      <div class="big-btns">
        <button type="button" class="big add" disabled={moveActive} title={addBtnTitle}>+</button>
        <button type="button" class="big remove" title="Letzten Punkt entfernen" on:click={removeLastPoint}>−</button>
      </div>
    </div>

    <!-- Bottom left: zoom -->
    <div class="zoom-stack">
      <button type="button" title="Heranzoomen" on:click={() => mapCanvas?.zoomIn()}>+</button>
      <button type="button" title="Herauszoomen" on:click={() => mapCanvas?.zoomOut()}>−</button>
      <button type="button" title="Auf Inhalt zoomen" on:click={() => mapCanvas?.fitToContent()}>◎</button>
    </div>

    <!-- Right sidebar -->
    <aside class="sidebar" class:collapsed={sidebarCollapsed}>
      <button
        class="collapse-btn"
        class:collapsed={sidebarCollapsed}
        title={sidebarCollapsed ? 'Sidebar ausklappen' : 'Sidebar einklappen'}
        on:click={() => (sidebarCollapsed = !sidebarCollapsed)}
      >{sidebarCollapsed ? '‹' : '›'}</button>

      {#if !sidebarCollapsed}
        <div class="sidebar-inner">

          <div class="sb-section">
            <span class="sb-label">Status</span>
            <div class="sb-row">
              <span class="muted">{$mapStore.dirty ? 'Ungespeichert' : 'Synchron'}</span>
            </div>
            <div class="sb-actions">
              <button type="button" class="sb-btn" title="Neu laden" disabled={busy} on:click={loadMap}>↺ Laden</button>
              <button type="button" class="sb-btn primary" class:unsaved={$mapStore.dirty} disabled={busy || !$mapStore.dirty} on:click={saveMap}>Speichern</button>
            </div>
          </div>

          <div class="sb-section">
            <span class="sb-label">Karte</span>
            <select class="sb-select" disabled title="Kartenauswahl — noch nicht verfügbar">
              <option>Hauptgarten</option>
            </select>
          </div>

          <div class="sb-section">
            <span class="sb-label">Import / Export</span>
            <div class="sb-actions">
              <button type="button" class="sb-btn" on:click={importMap}>↑ Import</button>
              <button type="button" class="sb-btn" on:click={exportMap}>↓ Export</button>
            </div>
          </div>

          <div class="sb-section">
            <span class="sb-label">Inhalt</span>
            <div class="sb-stat"><span>Perimeter</span><strong>{$mapStore.map.perimeter.length} Punkte</strong></div>
            <div class="sb-stat"><span>Dock</span><strong>{$mapStore.map.dock.length > 0 ? 'Gesetzt' : '—'}</strong></div>
            <div class="sb-stat"><span>Zonen</span><strong>{$mapStore.map.zones.length}</strong></div>
            <div class="sb-stat"><span>NoGo</span><strong>{$mapStore.map.exclusions.length}</strong></div>
          </div>

        </div>
      {/if}
    </aside>
  </div>
</main>

<style>
  .page { height: 100%; }

  .map-stage {
    position: relative;
    height: 100%;
    background: #070d18;
  }

  /* ── Bottom center toolbar ── */
  .toolbar-wrap {
    position: absolute;
    bottom: 1rem;
    left: 50%;
    transform: translateX(-50%);
    z-index: 10;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.4rem;
  }

  .layer-bar {
    display: flex;
    align-items: center;
    gap: 0.2rem;
    padding: 0.28rem 0.45rem;
    background: rgba(10, 16, 32, 0.94);
    border: 1px solid #1e3a5f;
    border-radius: 0.6rem;
    backdrop-filter: blur(6px);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
  }

  .layer-bar button {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    padding: 0.28rem 0.55rem;
    border-radius: 0.38rem;
    border: 1px solid transparent;
    background: transparent;
    color: #94a3b8;
    font-size: 0.72rem;
    font-weight: 600;
    cursor: pointer;
    white-space: nowrap;
  }

  .layer-bar button:hover { background: rgba(30, 58, 95, 0.4); color: #e2e8f0; }
  .layer-bar button.active { background: rgba(30, 58, 95, 0.35); border-color: #2563eb; color: #93c5fd; }
  .layer-bar button small { color: #475569; font-weight: 400; }

  .layer-dot {
    width: 0.45rem;
    height: 0.45rem;
    border-radius: 50%;
    flex-shrink: 0;
  }
  .layer-dot.perimeter { background: #2563eb; }
  .layer-dot.zone      { background: #0891b2; }
  .layer-dot.nogo      { background: #dc2626; }

  .item-bar {
    display: flex;
    align-items: center;
    gap: 0.25rem;
    padding: 0.22rem 0.4rem;
    background: rgba(10, 16, 32, 0.94);
    border: 1px solid #1e3a5f;
    border-radius: 0.5rem;
    flex-wrap: wrap;
    max-width: 380px;
  }

  .item-btn {
    padding: 0.22rem 0.5rem;
    border-radius: 0.35rem;
    border: 1px solid #1a2a40;
    background: #0a1020;
    color: #64748b;
    font-size: 0.62rem;
    font-weight: 600;
    cursor: pointer;
  }

  .item-btn:hover { border-color: #0891b2; color: #67e8f9; }
  .item-btn.active { border-color: #0891b2; background: #082f49; color: #67e8f9; }
  .item-btn.nogo:hover { border-color: #dc2626; color: #fca5a5; }
  .item-btn.nogo.active { border-color: #dc2626; background: #450a0a; color: #fca5a5; }

  .item-new {
    padding: 0.22rem 0.5rem;
    border-radius: 0.35rem;
    border: 1px solid #1e3a5f;
    background: transparent;
    color: #475569;
    font-size: 0.62rem;
    font-weight: 600;
    cursor: pointer;
  }

  .item-new:hover { color: #94a3b8; border-color: #334155; }

  .divider {
    width: 1px; height: 1.1rem;
    background: #1e3a5f;
    margin: 0 0.12rem;
    flex-shrink: 0;
  }

  .big-btns {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.4rem;
    width: 100%;
  }

  .big {
    height: 2.6rem;
    border-radius: 0.5rem;
    border: 1px solid transparent;
    font-size: 1.4rem;
    font-weight: 700;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .big:disabled { opacity: 0.35; cursor: default; }

  .big.add {
    background: rgba(12, 26, 58, 0.94);
    border-color: #2563eb;
    color: #93c5fd;
  }

  .big.add:hover:not(:disabled) { background: rgba(30, 58, 95, 0.94); }

  .big.remove {
    background: rgba(69, 10, 10, 0.94);
    border-color: #dc2626;
    color: #fca5a5;
  }

  .big.remove:hover { background: rgba(100, 15, 15, 0.94); }

  .info {
    font-size: 0.6rem;
    color: #4ade80;
    white-space: nowrap;
    padding: 0 0.25rem;
  }

  /* ── Bottom left zoom ── */
  .zoom-stack {
    position: absolute;
    bottom: 1rem;
    left: 0.75rem;
    z-index: 10;
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
  }

  .zoom-stack button {
    width: 1.8rem;
    height: 1.8rem;
    border-radius: 0.4rem;
    border: 1px solid #1e3a5f;
    background: rgba(15, 24, 41, 0.94);
    color: #60a5fa;
    font-size: 0.88rem;
    font-weight: 700;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .zoom-stack button:hover { background: rgba(30, 58, 95, 0.96); color: #93c5fd; }

  /* ── Right sidebar ── */
  .sidebar {
    position: absolute;
    top: 0;
    right: 0;
    bottom: 0;
    width: 220px;
    z-index: 5;
    padding-left: 0.55rem;
  }

  .sidebar.collapsed { width: 0; }

  .collapse-btn {
    position: absolute;
    left: calc(0.55rem - 14px);
    top: 50%;
    transform: translateY(-50%);
    z-index: 10;
    width: 14px;
    height: 44px;
    background: #0f1829;
    border: 1px solid #1e3a5f;
    border-right: none;
    color: #475569;
    cursor: pointer;
    border-radius: 6px 0 0 6px;
    font-size: 13px;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0;
  }

  .collapse-btn.collapsed {
    border-right: 1px solid #1e3a5f;
    border-left: none;
    border-radius: 0 6px 6px 0;
  }

  .collapse-btn:hover { color: #60a5fa; }

  .sidebar-inner {
    display: flex;
    flex-direction: column;
    height: 100%;
    background: #0a1020;
    border: 1px solid #1e3a5f;
    border-radius: 0.75rem;
    overflow: hidden;
    box-shadow: 0 18px 40px rgba(0, 0, 0, 0.28);
  }

  .sb-section {
    display: grid;
    gap: 0.35rem;
    padding: 0.55rem 0.65rem;
    border-bottom: 1px solid #0f1829;
  }

  .sb-label {
    color: #7a8da8;
    font-size: 0.59rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .muted { color: #94a3b8; font-size: 0.68rem; }

  .sb-row { display: flex; align-items: center; gap: 0.4rem; }

  .sb-actions {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.3rem;
  }

  .sb-btn {
    padding: 0.35rem 0.4rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.4rem;
    background: #0f1829;
    color: #94a3b8;
    font-size: 0.62rem;
    font-weight: 600;
    cursor: pointer;
    text-align: center;
  }

  .sb-btn:hover:not(:disabled) { background: #0c1a3a; color: #e2e8f0; }
  .sb-btn:disabled { opacity: 0.4; cursor: not-allowed; }

  .sb-btn.primary {
    border-color: #2563eb;
    background: #0c1a3a;
    color: #93c5fd;
  }

  .sb-btn.unsaved {
    border-color: #d97706;
    background: rgba(28, 18, 0, 0.6);
    color: #fbbf24;
  }

  .sb-select {
    width: 100%;
    padding: 0.3rem 0.4rem;
    border: 1px solid #1a2a40;
    border-radius: 0.4rem;
    background: #0a1020;
    color: #dce8e8;
    font-size: 0.68rem;
  }

  .sb-select:disabled { opacity: 0.5; }

  .sb-stat {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    font-size: 0.62rem;
    color: #64748b;
  }

  .sb-stat strong {
    color: #94a3b8;
    font-size: 0.64rem;
  }
</style>
