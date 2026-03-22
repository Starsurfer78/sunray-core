<script setup lang="ts">
import { ref, reactive, computed, onMounted, onUnmounted, watch } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'

// ── Types ──────────────────────────────────────────────────────────────────────
type Pt = [number, number]  // [x, y] in local metres (east=+x, north=+y)

interface Origin { lat: number; lon: number }

interface MapData {
  perimeter:  Pt[]
  mow:        Pt[]
  dock:       Pt[]       // single station point [x,y]
  dockPath:   Pt[]       // open polyline: approach waypoints leading to dock
  exclusions: Pt[][]
  obstacles:  Pt[]
  origin?:    Origin
}

type Tool = 'select' | 'perimeter' | 'exclusion' | 'dockPath' | 'obstacle' | 'delete'

// exIdx sentinel values for vertex identity
const EX_PERIMETER = -1
const EX_DOCK      = -3
const EX_DOCKPATH  = -4
const EX_OBSTACLE  = -2

type VertexRef = { kind: Tool; exIdx: number; idx: number }

const OBSTACLE_RADIUS_M = 0.5

// ── State ──────────────────────────────────────────────────────────────────────
const canvas    = ref<HTMLCanvasElement | null>(null)
const fileInput = ref<HTMLInputElement | null>(null)

const mapData    = reactive<MapData>({ perimeter: [], mow: [], dock: [], dockPath: [], exclusions: [], obstacles: [] })
const origin     = reactive<{ lat: string; lon: string }>({ lat: '', lon: '' })
const activeTool = ref<Tool>('select')
const drawingPts = ref<Pt[]>([])
const mouseLocal = ref<Pt>([0, 0])
const status     = ref('')
const dirty      = ref(false)

// ── Canvas viewport ────────────────────────────────────────────────────────────
let scale   = 20
let originX = 400
let originY = 300
let panStart:   { cx: number; cy: number; ox: number; oy: number } | null = null
let dragTarget: VertexRef | null = null
const SNAP_PX   = 12
const DRAG_PX   = 12

// ── Coordinate helpers ─────────────────────────────────────────────────────────
function toCanvas(mx: number, my: number): [number, number] {
  return [originX + mx * scale, originY - my * scale]
}
function fromCanvas(cx: number, cy: number): [number, number] {
  return [(cx - originX) / scale, (originY - cy) / scale]
}

// ── Draw helpers ───────────────────────────────────────────────────────────────
function drawArrow(ctx: CanvasRenderingContext2D, x1: number, y1: number, x2: number, y2: number) {
  const mx = (x1 + x2) / 2
  const my = (y1 + y2) / 2
  const ang = Math.atan2(y2 - y1, x2 - x1)
  const size = 6
  ctx.save()
  ctx.translate(mx, my)
  ctx.rotate(ang)
  ctx.beginPath()
  ctx.moveTo(size, 0)
  ctx.lineTo(-size, -size * 0.5)
  ctx.lineTo(-size, size * 0.5)
  ctx.closePath()
  ctx.fill()
  ctx.restore()
}

// ── Draw ───────────────────────────────────────────────────────────────────────
function draw() {
  const el = canvas.value
  if (!el) return
  const ctx = el.getContext('2d')!
  const W = el.width, H = el.height

  ctx.clearRect(0, 0, W, H)

  // Grid
  ctx.strokeStyle = '#1f2937'
  ctx.lineWidth   = 1
  const gridStep = scale >= 10 ? 5 : 10
  const gridPx   = gridStep * scale
  const startX   = originX % gridPx
  const startY   = originY % gridPx
  for (let x = startX; x < W; x += gridPx) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke() }
  for (let y = startY; y < H; y += gridPx) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke() }

  // Origin cross
  const [ox, oy] = toCanvas(0, 0)
  ctx.strokeStyle = '#374151'; ctx.lineWidth = 1
  ctx.beginPath(); ctx.moveTo(ox - 10, oy); ctx.lineTo(ox + 10, oy); ctx.stroke()
  ctx.beginPath(); ctx.moveTo(ox, oy - 10); ctx.lineTo(ox, oy + 10); ctx.stroke()

  // Mow points
  ctx.fillStyle = '#4b5563'
  for (const [mx, my] of mapData.mow) {
    const [cx, cy] = toCanvas(mx, my)
    ctx.beginPath(); ctx.arc(cx, cy, 2, 0, Math.PI * 2); ctx.fill()
  }

  // Exclusion zones
  for (const excl of mapData.exclusions) {
    if (excl.length < 2) continue
    ctx.beginPath()
    const [x0, y0] = toCanvas(excl[0][0], excl[0][1]); ctx.moveTo(x0, y0)
    for (let i = 1; i < excl.length; i++) { const [cx, cy] = toCanvas(excl[i][0], excl[i][1]); ctx.lineTo(cx, cy) }
    ctx.closePath()
    ctx.fillStyle = 'rgba(239,68,68,0.18)'; ctx.strokeStyle = '#ef4444'; ctx.lineWidth = 1.5
    ctx.fill(); ctx.stroke()
    ctx.fillStyle = '#ef4444'
    for (const [mx, my] of excl) { const [cx, cy] = toCanvas(mx, my); ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill() }
  }

  // Perimeter
  if (mapData.perimeter.length >= 2) {
    ctx.beginPath()
    const [x0, y0] = toCanvas(mapData.perimeter[0][0], mapData.perimeter[0][1]); ctx.moveTo(x0, y0)
    for (let i = 1; i < mapData.perimeter.length; i++) { const [cx, cy] = toCanvas(mapData.perimeter[i][0], mapData.perimeter[i][1]); ctx.lineTo(cx, cy) }
    ctx.closePath()
    ctx.fillStyle = 'rgba(34,197,94,0.12)'; ctx.strokeStyle = '#22c55e'; ctx.lineWidth = 2
    ctx.fill(); ctx.stroke()
    ctx.fillStyle = '#22c55e'
    for (const [mx, my] of mapData.perimeter) { const [cx, cy] = toCanvas(mx, my); ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill() }
  }

  // Dock path (sky-blue dashed polyline with arrows)
  if (mapData.dockPath.length >= 2) {
    ctx.strokeStyle = '#38bdf8'; ctx.lineWidth = 2; ctx.setLineDash([6, 4])
    ctx.beginPath()
    const [px0, py0] = toCanvas(mapData.dockPath[0][0], mapData.dockPath[0][1]); ctx.moveTo(px0, py0)
    for (let i = 1; i < mapData.dockPath.length; i++) {
      const [cx, cy] = toCanvas(mapData.dockPath[i][0], mapData.dockPath[i][1]); ctx.lineTo(cx, cy)
    }
    ctx.stroke(); ctx.setLineDash([])
    // Arrows at segment midpoints
    ctx.fillStyle = '#38bdf8'
    for (let i = 0; i < mapData.dockPath.length - 1; i++) {
      const [ax, ay] = toCanvas(mapData.dockPath[i][0], mapData.dockPath[i][1])
      const [bx, by] = toCanvas(mapData.dockPath[i + 1][0], mapData.dockPath[i + 1][1])
      drawArrow(ctx, ax, ay, bx, by)
    }
    // Waypoint circles
    ctx.strokeStyle = '#38bdf8'; ctx.lineWidth = 1.5
    for (const [mx, my] of mapData.dockPath) {
      const [cx, cy] = toCanvas(mx, my)
      ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2)
      ctx.fillStyle = '#0c4a6e'; ctx.fill(); ctx.stroke()
    }
  } else if (mapData.dockPath.length === 1) {
    const [cx, cy] = toCanvas(mapData.dockPath[0][0], mapData.dockPath[0][1])
    ctx.strokeStyle = '#38bdf8'; ctx.lineWidth = 1.5; ctx.fillStyle = '#0c4a6e'
    ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill(); ctx.stroke()
  }

  // Dock station: last point of dockPath (or legacy mapData.dock[0])
  const dockStation = mapData.dockPath.length > 0
    ? mapData.dockPath[mapData.dockPath.length - 1]
    : mapData.dock.length > 0 ? mapData.dock[0] : null
  if (dockStation) {
    const [dx, dy] = toCanvas(dockStation[0], dockStation[1])
    ctx.fillStyle = '#3b82f6'; ctx.strokeStyle = '#93c5fd'; ctx.lineWidth = 2
    ctx.beginPath(); ctx.arc(dx, dy, 9, 0, Math.PI * 2); ctx.fill(); ctx.stroke()
    ctx.fillStyle = '#fff'; ctx.font = 'bold 10px sans-serif'
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle'
    ctx.fillText('D', dx, dy)
  }

  // Obstacles
  for (const [mx, my] of mapData.obstacles) {
    const [cx, cy] = toCanvas(mx, my)
    const rPx = Math.max(4, OBSTACLE_RADIUS_M * scale)
    ctx.beginPath(); ctx.arc(cx, cy, rPx, 0, Math.PI * 2)
    ctx.fillStyle = 'rgba(217,119,6,0.25)'; ctx.strokeStyle = '#d97706'; ctx.lineWidth = 1.5
    ctx.fill(); ctx.stroke()
    ctx.strokeStyle = '#fbbf24'; ctx.lineWidth = 1
    ctx.beginPath(); ctx.moveTo(cx - 5, cy); ctx.lineTo(cx + 5, cy); ctx.stroke()
    ctx.beginPath(); ctx.moveTo(cx, cy - 5); ctx.lineTo(cx, cy + 5); ctx.stroke()
  }

  // Drag highlight: ring around the vertex being dragged
  if (dragTarget) {
    const list = getVertexList(dragTarget)
    if (list && dragTarget.idx < list.length) {
      const [vx, vy] = toCanvas(list[dragTarget.idx][0], list[dragTarget.idx][1])
      ctx.strokeStyle = '#f8fafc'; ctx.lineWidth = 2
      ctx.beginPath(); ctx.arc(vx, vy, 8, 0, Math.PI * 2); ctx.stroke()
    }
  }

  // Currently drawing preview (polygon or polyline)
  if (drawingPts.value.length > 0) {
    const tool  = activeTool.value
    const color = tool === 'exclusion' ? '#f87171' : tool === 'dockPath' ? '#7dd3fc' : '#86efac'
    ctx.strokeStyle = color; ctx.lineWidth = 1.5; ctx.setLineDash([5, 4])
    ctx.beginPath()
    const [fx, fy] = toCanvas(drawingPts.value[0][0], drawingPts.value[0][1]); ctx.moveTo(fx, fy)
    for (let i = 1; i < drawingPts.value.length; i++) {
      const [cx, cy] = toCanvas(drawingPts.value[i][0], drawingPts.value[i][1]); ctx.lineTo(cx, cy)
    }
    const [mcx, mcy] = toCanvas(mouseLocal.value[0], mouseLocal.value[1]); ctx.lineTo(mcx, mcy)
    ctx.stroke(); ctx.setLineDash([])
    ctx.fillStyle = color
    for (const [ptx, pty] of drawingPts.value) {
      const [cx, cy] = toCanvas(ptx, pty)
      ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill()
    }
    // Snap ring only for closed polygons (not for dockPath polyline)
    if (tool !== 'dockPath') {
      const [s0x, s0y] = toCanvas(drawingPts.value[0][0], drawingPts.value[0][1])
      ctx.strokeStyle = color; ctx.lineWidth = 1.5
      ctx.beginPath(); ctx.arc(s0x, s0y, SNAP_PX, 0, Math.PI * 2); ctx.stroke()
    }
  }
}

// ── Vertex helpers ─────────────────────────────────────────────────────────────
function getVertexList(ref: VertexRef): Pt[] {
  if (ref.kind === 'perimeter' || ref.exIdx === EX_PERIMETER) return mapData.perimeter
  if (ref.exIdx === EX_DOCK) return mapData.dock
  if (ref.kind === 'dockPath'  || ref.exIdx === EX_DOCKPATH)  return mapData.dockPath
  if (ref.exIdx === EX_OBSTACLE) return mapData.obstacles
  if (ref.exIdx >= 0) return mapData.exclusions[ref.exIdx]
  return mapData.perimeter
}

function findNearestVertex(cx: number, cy: number, thresh: number): VertexRef | null {
  let best: VertexRef | null = null
  let bestDist = thresh + 1

  function check(pts: Pt[], kind: Tool, exIdx: number) {
    for (let i = 0; i < pts.length; i++) {
      const [px, py] = toCanvas(pts[i][0], pts[i][1])
      const d = Math.sqrt((cx - px) ** 2 + (cy - py) ** 2)
      if (d < bestDist) { bestDist = d; best = { kind, exIdx, idx: i } }
    }
  }

  check(mapData.perimeter, 'perimeter', EX_PERIMETER)
  check(mapData.dock,      'select',    EX_DOCK)  // dock: no dedicated tool, use exIdx sentinel
  check(mapData.dockPath,  'dockPath',  EX_DOCKPATH)
  check(mapData.obstacles, 'obstacle',  EX_OBSTACLE)
  mapData.exclusions.forEach((ex, i) => check(ex, 'exclusion', i))

  return best
}

// ── Load / Save ───────────────────────────────────────────────────────────────
async function loadMap() {
  try {
    const res = await fetch('/api/map')
    if (!res.ok) { status.value = `Laden fehlgeschlagen: HTTP ${res.status}`; return }
    const j = await res.json()
    mapData.perimeter  = j.perimeter  ?? []
    mapData.mow        = j.mow        ?? []
    mapData.dock       = j.dock       ?? []
    mapData.dockPath   = j.dockPath   ?? []
    mapData.exclusions = j.exclusions ?? []
    mapData.obstacles  = j.obstacles  ?? []
    if (j.origin) { origin.lat = String(j.origin.lat); origin.lon = String(j.origin.lon) }
    dirty.value  = false
    status.value = 'Karte geladen.'
    fitView()
    draw()
  } catch (e) {
    status.value = `Ladefehler: ${e}`
  }
}

async function saveMap() {
  try {
    // Derive dock station from last dockPath point (C++ uses dock[0] as target)
    const derivedDock = mapData.dockPath.length > 0
      ? [mapData.dockPath[mapData.dockPath.length - 1]]
      : mapData.dock
    const res = await fetch('/api/map', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        perimeter:  mapData.perimeter,
        mow:        mapData.mow,
        dock:       derivedDock,
        dockPath:   mapData.dockPath,
        exclusions: mapData.exclusions,
        obstacles:  mapData.obstacles,
        ...(origin.lat && origin.lon
          ? { origin: { lat: parseFloat(origin.lat), lon: parseFloat(origin.lon) } }
          : {}),
      }),
    })
    const j = await res.json()
    if (j.ok) { dirty.value = false; status.value = 'Karte gespeichert.' }
    else       { status.value = `Speichern fehlgeschlagen: ${j.error ?? 'unbekannt'}` }
  } catch (e) {
    status.value = `Speicherfehler: ${e}`
  }
}

// ── Fit view ──────────────────────────────────────────────────────────────────
function fitView() {
  const el = canvas.value
  if (!el) return
  const allPts: Pt[] = [
    ...mapData.perimeter, ...mapData.dock, ...mapData.dockPath,
    ...mapData.mow, ...mapData.exclusions.flat(), ...mapData.obstacles,
  ]
  if (allPts.length === 0) { originX = el.width / 2; originY = el.height / 2; scale = 20; return }
  const xs = allPts.map(p => p[0]), ys = allPts.map(p => p[1])
  const minX = Math.min(...xs), maxX = Math.max(...xs)
  const minY = Math.min(...ys), maxY = Math.max(...ys)
  const margin = 60
  scale   = Math.min((el.width - margin * 2) / (maxX - minX || 10), (el.height - margin * 2) / (maxY - minY || 10))
  scale   = Math.max(2, Math.min(scale, 100))
  originX = el.width  / 2 - ((minX + maxX) / 2) * scale
  originY = el.height / 2 + ((minY + maxY) / 2) * scale
}

// ── GeoJSON Import / Export ───────────────────────────────────────────────────
async function exportGeoJson() {
  try {
    const res = await fetch('/api/map/geojson')
    if (!res.ok) { status.value = `GeoJSON Export fehlgeschlagen: HTTP ${res.status}`; return }
    const blob = await res.blob()
    const url  = URL.createObjectURL(blob)
    const a    = document.createElement('a')
    a.href = url; a.download = 'sunray-map.geojson'; a.click()
    URL.revokeObjectURL(url)
    status.value = 'GeoJSON exportiert.'
  } catch (e) { status.value = `GeoJSON Export Fehler: ${e}` }
}

function triggerImport() { fileInput.value?.click() }

async function onFileSelected(e: Event) {
  const file = (e.target as HTMLInputElement).files?.[0]
  if (!file) return
  try {
    const text = await file.text()
    const res  = await fetch('/api/map/geojson', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: text,
    })
    const j = await res.json()
    if (j.ok) { status.value = 'GeoJSON importiert — Karte wird neu geladen…'; await loadMap() }
    else       { status.value = `GeoJSON Import fehlgeschlagen: ${j.error ?? 'unbekannt'}` }
  } catch (e) { status.value = `GeoJSON Import Fehler: ${e}` }
  if (fileInput.value) fileInput.value.value = ''
}

function useGpsAsOrigin() {
  const { telemetry: tele } = useTelemetry()
  const lat = tele.value.gps_lat, lon = tele.value.gps_lon
  if (lat === 0 && lon === 0) { status.value = 'Kein GPS-Fix verfügbar (lat=0, lon=0).'; return }
  origin.lat = lat.toFixed(7); origin.lon = lon.toFixed(7)
  dirty.value = true
  status.value = `Ursprung aus GPS: ${origin.lat}, ${origin.lon}`
}

// ── Tool actions ──────────────────────────────────────────────────────────────
const perimeterExists = computed(() => mapData.perimeter.length >= 3)
const isDrawing       = computed(() => drawingPts.value.length > 0)

function selectTool(t: Tool) {
  if (t === 'dockPath' && !perimeterExists.value) {
    status.value = 'Bitte zuerst einen Perimeter anlegen.'
    return
  }
  activeTool.value = t; drawingPts.value = []; draw()
}

function undoLastPoint() {
  if (drawingPts.value.length === 0) return
  drawingPts.value = drawingPts.value.slice(0, -1)
  status.value = drawingPts.value.length > 0
    ? `Letzter Punkt entfernt (${drawingPts.value.length} verbleibend)`
    : 'Alle Zeichenpunkte entfernt.'
  draw()
}

function clearPerimeter()  { mapData.perimeter = []; dirty.value = true; draw() }
function clearExclusions() { mapData.exclusions = []; dirty.value = true; draw() }
function clearDockPath()   { mapData.dockPath = []; mapData.dock = []; dirty.value = true; draw(); status.value = 'Dock-Pfad gelöscht.' }

async function clearObstacles() {
  mapData.obstacles = []; dirty.value = true; draw()
  try { await fetch('/api/sim/obstacles_clear', { method: 'POST' }) } catch { /* sim may not be running */ }
  status.value = 'Hindernisse gelöscht.'
}

// Finish current polygon and commit
function closePolygon() {
  if (drawingPts.value.length < 3) { status.value = 'Mindestens 3 Punkte erforderlich.'; return }
  if (activeTool.value === 'perimeter') {
    mapData.perimeter = [...drawingPts.value]
    status.value = 'Perimeter gesetzt.'
  } else if (activeTool.value === 'exclusion') {
    mapData.exclusions = [...mapData.exclusions, [...drawingPts.value]]
    status.value = 'No-Go-Zone hinzugefügt.'
  }
  drawingPts.value = []; dirty.value = true; draw()
}

// Finish dock path polyline
function finishDockPath() {
  if (drawingPts.value.length < 2) { status.value = 'Mindestens 2 Punkte erforderlich.'; return }
  mapData.dockPath = [...drawingPts.value]
  drawingPts.value = []; dirty.value = true
  status.value = `Dock-Pfad gesetzt (${mapData.dockPath.length} Wegpunkte).`
  draw()
}

// ── Delete nearest vertex ─────────────────────────────────────────────────────
function deleteNearestVertex(cx: number, cy: number) {
  const ref = findNearestVertex(cx, cy, SNAP_PX)
  if (!ref) { status.value = 'Kein Punkt in der Nähe.'; return }

  if (ref.exIdx === EX_OBSTACLE) {
    mapData.obstacles.splice(ref.idx, 1)
  } else if (ref.exIdx === EX_DOCK) {
    mapData.dock = []
  } else if (ref.exIdx === EX_DOCKPATH) {
    mapData.dockPath.splice(ref.idx, 1)
  } else if (ref.exIdx >= 0) {
    mapData.exclusions[ref.exIdx].splice(ref.idx, 1)
    if (mapData.exclusions[ref.exIdx].length < 3) mapData.exclusions.splice(ref.exIdx, 1)
  } else {
    mapData.perimeter.splice(ref.idx, 1)
  }
  dirty.value = true; draw()
}

// ── Canvas event handlers ──────────────────────────────────────────────────────
function canvasClick(e: MouseEvent) {
  // Suppress click if it was the end of a drag
  if (dragTarget) return

  const el   = canvas.value!
  const rect = el.getBoundingClientRect()
  const cx   = e.clientX - rect.left
  const cy   = e.clientY - rect.top
  const [mx, my] = fromCanvas(cx, cy)

  if (activeTool.value === 'obstacle') {
    mapData.obstacles = [...mapData.obstacles, [mx, my]]; dirty.value = true
    status.value = `Hindernis gesetzt (${mx.toFixed(1)}, ${my.toFixed(1)}) — r=${OBSTACLE_RADIUS_M} m`
    draw()
    fetch('/api/sim/obstacle', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ x: mx, y: my, radius: OBSTACLE_RADIUS_M }),
    }).catch(() => {})
    return
  }

  if (activeTool.value === 'delete') { deleteNearestVertex(cx, cy); return }

  if (activeTool.value === 'dockPath') {
    drawingPts.value = [...drawingPts.value, [mx, my]]; draw(); return
  }

  if (activeTool.value === 'perimeter' || activeTool.value === 'exclusion') {
    // Snap-to-first: close polygon if click near first point
    if (drawingPts.value.length >= 3) {
      const [fx, fy] = toCanvas(drawingPts.value[0][0], drawingPts.value[0][1])
      const dx = cx - fx, dy = cy - fy
      if (Math.sqrt(dx * dx + dy * dy) < SNAP_PX) { closePolygon(); return }
    }
    drawingPts.value = [...drawingPts.value, [mx, my]]; draw()
  }
}

function canvasDblClick(e: MouseEvent) {
  if (activeTool.value === 'perimeter' || activeTool.value === 'exclusion') {
    e.preventDefault(); closePolygon()
  } else if (activeTool.value === 'dockPath') {
    e.preventDefault(); finishDockPath()
  }
}

function canvasMouseMove(e: MouseEvent) {
  const el   = canvas.value!
  const rect = el.getBoundingClientRect()
  const cx   = e.clientX - rect.left
  const cy   = e.clientY - rect.top
  mouseLocal.value = fromCanvas(cx, cy)

  if (dragTarget) {
    // Move the dragged vertex
    const list = getVertexList(dragTarget)
    if (list && dragTarget.idx < list.length) {
      list[dragTarget.idx] = [...mouseLocal.value] as Pt
      dirty.value = true
    }
  } else if (panStart) {
    originX = panStart.ox + (cx - panStart.cx)
    originY = panStart.oy + (cy - panStart.cy)
  }
  draw()
}

function canvasMouseDown(e: MouseEvent) {
  if (e.button === 0 && activeTool.value === 'select') {
    const el   = canvas.value!
    const rect = el.getBoundingClientRect()
    const cx   = e.clientX - rect.left
    const cy   = e.clientY - rect.top
    // Try vertex drag first
    const ref = findNearestVertex(cx, cy, DRAG_PX)
    if (ref) {
      dragTarget = ref
      e.preventDefault()
      return
    }
    // Otherwise pan
    panStart = { cx, cy, ox: originX, oy: originY }
    e.preventDefault()
  } else if (e.button === 1) {
    const el   = canvas.value!
    const rect = el.getBoundingClientRect()
    panStart = { cx: e.clientX - rect.left, cy: e.clientY - rect.top, ox: originX, oy: originY }
    e.preventDefault()
  }
}

function canvasMouseUp(e: MouseEvent) {
  if (dragTarget) {
    dragTarget = null
    e.preventDefault()
    return
  }
  if (panStart) {
    const el   = canvas.value!
    const rect = el.getBoundingClientRect()
    const movedX = Math.abs(e.clientX - rect.left - panStart.cx)
    const movedY = Math.abs(e.clientY - rect.top  - panStart.cy)
    // Barely moved in select mode → treat as click (let canvasClick handle it)
    if (movedX < 3 && movedY < 3 && e.button === 0 && activeTool.value === 'select') {
      panStart = null; return
    }
    panStart = null
  }
}

function canvasWheel(e: WheelEvent) {
  e.preventDefault()
  const el   = canvas.value!
  const rect = el.getBoundingClientRect()
  const cx   = e.clientX - rect.left
  const cy   = e.clientY - rect.top
  const factor   = e.deltaY < 0 ? 1.15 : 1 / 1.15
  const newScale = Math.max(1, Math.min(scale * factor, 200))
  originX = cx - (cx - originX) * (newScale / scale)
  originY = cy - (cy - originY) * (newScale / scale)
  scale = newScale; draw()
}

// ── Canvas resize ─────────────────────────────────────────────────────────────
let resizeObs: ResizeObserver
function setupResize() {
  const el = canvas.value!
  resizeObs = new ResizeObserver(() => {
    const rect = el.getBoundingClientRect()
    el.width  = rect.width  || 800
    el.height = rect.height || 500
    draw()
  })
  resizeObs.observe(el)
  const rect = el.getBoundingClientRect()
  el.width  = rect.width  || 800
  el.height = rect.height || 500
}

// ── Keyboard shortcuts ────────────────────────────────────────────────────────
function onKeyDown(e: KeyboardEvent) {
  // Ignore if focus is in an input
  if (e.target instanceof HTMLInputElement) return
  if (e.key === 'Backspace' || e.key === 'Delete') {
    if (isDrawing.value) { e.preventDefault(); undoLastPoint() }
  }
  if (e.key === 'Escape') {
    if (isDrawing.value) { drawingPts.value = []; status.value = 'Zeichnung abgebrochen.'; draw() }
  }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
onMounted(async () => {
  setupResize()
  await loadMap()
  window.addEventListener('keydown', onKeyDown)
})
onUnmounted(() => {
  resizeObs?.disconnect()
  window.removeEventListener('keydown', onKeyDown)
})
watch(activeTool, draw)
</script>

<template>
  <div class="flex flex-col h-full gap-2">

    <!-- Hidden file input for GeoJSON import -->
    <input ref="fileInput" type="file" accept=".geojson,.json" class="hidden" @change="onFileSelected" />

    <!-- Toolbar -->
    <div class="flex items-center gap-2 flex-wrap bg-gray-900 border border-gray-800 rounded-lg p-2">

      <!-- Tool buttons -->
      <span class="text-xs text-gray-500 mr-1">Werkzeug:</span>
      <button
        v-for="[tool, label, title] in ([
          ['select',    'Zeiger',     'Punkte verschieben (Ziehen) · Karte verschieben (freier Bereich)'],
          ['perimeter', 'Perimeter',  'Perimeter zeichnen — Doppelklick oder Klick auf ersten Punkt zum Schließen'],
          ['exclusion', 'No-Go',      'No-Go-Zone zeichnen'],
          ['dockPath',  'Dock-Pfad',  'Anfahrtsweg zur Dock-Station zeichnen — letzter Punkt = Station · Doppelklick zum Abschließen'],
          ['obstacle',  'Hindernis',  'Simulations-Hindernis setzen (Kreis r=0.5 m)'],
          ['delete',    'Löschen',    'Nächsten gesetzten Punkt löschen'],
        ] as [Tool, string, string][])"
        :key="tool"
        :title="tool === 'dockPath' && !perimeterExists ? 'Bitte zuerst Perimeter anlegen' : title"
        :disabled="tool === 'dockPath' && !perimeterExists"
        @click="selectTool(tool)"
        :class="[
          'px-3 py-1 rounded text-xs font-medium transition-colors',
          tool === 'dockPath' && !perimeterExists
            ? 'bg-gray-800 text-gray-600 cursor-not-allowed'
            : activeTool === tool
              ? tool === 'perimeter' ? 'bg-green-700 text-green-100'
              : tool === 'exclusion' ? 'bg-red-700 text-red-100'
              : tool === 'dockPath'  ? 'bg-sky-700 text-sky-100'
              : tool === 'obstacle'  ? 'bg-amber-700 text-amber-100'
              : tool === 'delete'    ? 'bg-orange-700 text-orange-100'
              :                        'bg-gray-600 text-gray-100'
              : 'bg-gray-800 text-gray-300 hover:bg-gray-700'
        ]"
      >{{ label }}</button>

      <div class="w-px h-5 bg-gray-700 mx-1" />

      <!-- Clear buttons -->
      <button @click="clearPerimeter"  title="Perimeter löschen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ Perimeter</button>
      <button @click="clearExclusions" title="Alle No-Go-Zonen löschen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ No-Go</button>
      <button @click="clearDockPath"   title="Dock-Pfad löschen (letzter Punkt = Dock-Station)"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ Dock-Pfad</button>
      <button @click="clearObstacles"  title="Alle Hindernisse löschen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ Hindernisse</button>

      <div class="w-px h-5 bg-gray-700 mx-1" />

      <!-- View -->
      <button @click="fitView(); draw()" title="Ansicht anpassen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⊡ Fit</button>

      <div class="flex-1" />

      <!-- GeoJSON Import / Export -->
      <button @click="triggerImport"
        class="px-3 py-1 rounded text-xs bg-gray-700 text-gray-200 hover:bg-gray-600" title="GeoJSON-Datei importieren">
        ↑ GeoJSON
      </button>
      <button @click="exportGeoJson"
        class="px-3 py-1 rounded text-xs bg-gray-700 text-gray-200 hover:bg-gray-600" title="Als GeoJSON herunterladen">
        ↓ GeoJSON
      </button>

      <div class="w-px h-5 bg-gray-700 mx-1" />

      <!-- Save / Load -->
      <button @click="loadMap"
        class="px-3 py-1 rounded text-xs bg-gray-700 text-gray-200 hover:bg-gray-600">Laden</button>
      <button @click="saveMap"
        :class="['px-3 py-1 rounded text-xs font-medium transition-colors',
          dirty ? 'bg-green-700 hover:bg-green-600 text-white' : 'bg-gray-700 text-gray-400']">
        {{ dirty ? 'Speichern *' : 'Gespeichert' }}
      </button>
    </div>

    <!-- Origin row -->
    <div class="flex items-center gap-2 bg-gray-900 border border-gray-800 rounded-lg px-3 py-1.5 text-xs">
      <span class="text-gray-500">GPS-Ursprung:</span>
      <label class="text-gray-400">Lat</label>
      <input v-model="origin.lat" @input="dirty = true" type="number" step="0.000001" placeholder="0.000000"
        class="w-28 bg-gray-800 border border-gray-700 rounded px-2 py-0.5 text-gray-200 font-mono text-xs" />
      <label class="text-gray-400">Lon</label>
      <input v-model="origin.lon" @input="dirty = true" type="number" step="0.000001" placeholder="0.000000"
        class="w-28 bg-gray-800 border border-gray-700 rounded px-2 py-0.5 text-gray-200 font-mono text-xs" />
      <button @click="useGpsAsOrigin"
        class="px-2 py-0.5 rounded bg-blue-800 text-blue-200 hover:bg-blue-700 text-xs" title="Aktuellen GPS-Fix als Ursprung übernehmen">
        Von GPS
      </button>
      <span class="text-gray-600 ml-2">(benötigt für GeoJSON-Export; lokal → WGS84)</span>
    </div>

    <!-- Canvas area -->
    <div class="relative flex-1 min-h-0 bg-gray-950 border border-gray-800 rounded-lg overflow-hidden">
      <canvas
        ref="canvas"
        class="w-full h-full block"
        :style="activeTool === 'delete' ? 'cursor:not-allowed' : activeTool === 'select' ? 'cursor:default' : 'cursor:crosshair'"
        @click="canvasClick"
        @dblclick="canvasDblClick"
        @mousemove="canvasMouseMove"
        @mousedown="canvasMouseDown"
        @mouseup="canvasMouseUp"
        @wheel.prevent="canvasWheel"
        @contextmenu.prevent
      />

      <!-- Cursor coordinates -->
      <div class="absolute bottom-2 left-2 text-xs text-gray-500 select-none pointer-events-none font-mono">
        x={{ mouseLocal[0].toFixed(2) }} m &nbsp; y={{ mouseLocal[1].toFixed(2) }} m
      </div>

      <!-- Legend -->
      <div class="absolute top-2 right-2 text-xs text-gray-500 select-none pointer-events-none space-y-1">
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm bg-green-500 opacity-70" /> Perimeter</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm bg-red-500 opacity-70" /> No-Go</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-full bg-blue-500 opacity-90" /> Dock-Station (letzter Pfadpunkt)</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm bg-sky-400 opacity-70" /> Dock-Pfad</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-full bg-gray-500 opacity-70" /> Mäh-Pfad</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-full bg-amber-500 opacity-70" /> Hindernis (Sim)</div>
      </div>

      <!-- Drawing hints + inline undo -->
      <div v-if="activeTool === 'perimeter' || activeTool === 'exclusion'"
        class="absolute top-2 left-2 flex items-center gap-2 text-xs text-gray-400 bg-gray-900 bg-opacity-90 rounded px-2 py-1 select-none">
        <span class="pointer-events-none">Klicken: Punkt setzen &nbsp;·&nbsp; Doppelklick / ⊙ Erstpunkt: schließen &nbsp;·&nbsp; Esc: Abbruch</span>
        <button v-if="isDrawing" @click="undoLastPoint"
          class="pointer-events-auto px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600 font-mono"
          title="Letzten Punkt löschen (Backspace)">⌫ Letzten</button>
      </div>
      <div v-else-if="activeTool === 'dockPath'"
        class="absolute top-2 left-2 flex items-center gap-2 text-xs text-gray-400 bg-gray-900 bg-opacity-90 rounded px-2 py-1 select-none">
        <span class="pointer-events-none">Klicken: Wegpunkt &nbsp;·&nbsp; Doppelklick: abschließen (letzter Punkt = Dock-Station) &nbsp;·&nbsp; Esc: Abbruch</span>
        <button v-if="isDrawing" @click="undoLastPoint"
          class="pointer-events-auto px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600 font-mono"
          title="Letzten Punkt löschen (Backspace)">⌫ Letzten</button>
      </div>
      <div v-else-if="activeTool === 'select'"
        class="absolute top-2 left-2 text-xs text-gray-400 bg-gray-900 bg-opacity-80 rounded px-2 py-1 select-none pointer-events-none">
        Punkt ziehen: verschieben &nbsp;·&nbsp; Freier Bereich ziehen: Karte verschieben
      </div>
    </div>

    <!-- Status bar -->
    <div class="text-xs text-gray-500 px-1">
      {{ status || 'Bereit.' }}
      &nbsp;·&nbsp; Perimeter: {{ mapData.perimeter.length }} Punkte
      &nbsp;·&nbsp; No-Go: {{ mapData.exclusions.length }} Zone(n)
      &nbsp;·&nbsp; Dock-Pfad: {{ mapData.dockPath.length > 0 ? mapData.dockPath.length + ' Wegpunkte (letzer = Station)' : '—' }}
      &nbsp;·&nbsp; Mäh-Pfad: {{ mapData.mow.length }} (nur-lesen)
      &nbsp;·&nbsp; Hindernisse: {{ mapData.obstacles.length }}
    </div>

  </div>
</template>
