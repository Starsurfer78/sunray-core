<script setup lang="ts">
import { ref, reactive, computed, onMounted, onUnmounted, watch } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'
import { type MowPt, type MowPathSettings, DEFAULT_SETTINGS, computeMowPath } from '../composables/useMowPath'

// ── Types ──────────────────────────────────────────────────────────────────────
type Pt = [number, number]  // [x, y] in local metres (east=+x, north=+y)

interface Origin { lat: number; lon: number }

interface ZoneSettings {
  name:       string
  stripWidth: number        // metres
  angle:      number        // degrees 0..179
  edgeMowing: boolean
  edgeRounds: number
  speed:      number        // m/s
  pattern:    'stripe' | 'spiral'
}

interface Zone {
  id:       string
  polygon:  Pt[]
  order:    number          // legacy editor ordering until missions own sequencing
  settings: ZoneSettings
}

interface CapturePointMeta {
  fix_duration_ms: number
  sample_variance: number
  mean_accuracy: number
}

interface CaptureMeta {
  perimeter: CapturePointMeta[]
}

// MowPt is imported from useMowPath composable

interface MapData {
  perimeter:  Pt[]
  mow:        MowPt[]
  dock:       Pt[]       // single station point [x,y]
  dockPath:   Pt[]       // open polyline: approach waypoints leading to dock
  exclusions: Pt[][]
  obstacles:  Pt[]
  zones:      Zone[]
  origin?:    Origin
  captureMeta?: CaptureMeta
}

type Tool = 'select' | 'perimeter' | 'exclusion' | 'dockPath' | 'obstacle' | 'delete' | 'insert' | 'zone'

// exIdx sentinel values for vertex identity
const EX_PERIMETER = -1
const EX_DOCK      = -3
const EX_DOCKPATH  = -4
const EX_OBSTACLE  = -2

type VertexRef = { kind: Tool; exIdx: number; idx: number }

const OBSTACLE_RADIUS_M = 0.5

function emptyCapturePointMeta(): CapturePointMeta {
  return { fix_duration_ms: 0, sample_variance: 0, mean_accuracy: 0 }
}

// ── State ──────────────────────────────────────────────────────────────────────
const canvas    = ref<HTMLCanvasElement | null>(null)
const fileInput = ref<HTMLInputElement | null>(null)

const mapData    = reactive<MapData>({ perimeter: [], mow: [] as MowPt[], dock: [], dockPath: [], exclusions: [], obstacles: [], zones: [], captureMeta: { perimeter: [] } })
const origin     = reactive<{ lat: string; lon: string }>({ lat: '', lon: '' })
const activeTool = ref<Tool>('select')
const drawingPts = ref<Pt[]>([])
const mouseLocal = ref<Pt>([0, 0])
const status     = ref('')
const dirty      = ref(false)

// Zones panel state
const zonesOpen      = ref(true)
const selectedZoneId = ref<string | null>(null)
const editingZoneId  = ref<string | null>(null)

// Mähbahnen-Berechnung state
const pathPanelOpen  = ref(false)
const previewMow     = ref<MowPt[]>([])
const pathSettings   = reactive<MowPathSettings>({ ...DEFAULT_SETTINGS })
const pathZoneId     = ref<string | null>(null)  // Zone for which the path is computed

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
  ctx.strokeStyle = '#1e3a5f'
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

  // Saved mow points (grey = normal, dark-red = reverse segment)
  for (const mp of mapData.mow) {
    const [cx, cy] = toCanvas(mp.p[0], mp.p[1])
    ctx.fillStyle = mp.rev ? '#7f1d1d' : '#4b5563'
    ctx.beginPath(); ctx.arc(cx, cy, 2, 0, Math.PI * 2); ctx.fill()
  }

  // Preview mow path (cyan = forward, orange = reverse)
  if (previewMow.value.length > 1) {
    for (let i = 0; i < previewMow.value.length - 1; i++) {
      const a = previewMow.value[i]
      const b = previewMow.value[i + 1]
      const [ax, ay] = toCanvas(a.p[0], a.p[1])
      const [bx, by] = toCanvas(b.p[0], b.p[1])
      ctx.strokeStyle = b.rev ? '#fb923c' : '#22d3ee'
      ctx.lineWidth = 1.5
      ctx.setLineDash(b.rev ? [4, 3] : [])
      ctx.beginPath(); ctx.moveTo(ax, ay); ctx.lineTo(bx, by); ctx.stroke()
      ctx.setLineDash([])
      ctx.fillStyle = b.rev ? '#fb923c' : '#22d3ee'
      drawArrow(ctx, ax, ay, bx, by)
    }
    for (const mp of previewMow.value) {
      const [cx, cy] = toCanvas(mp.p[0], mp.p[1])
      ctx.fillStyle = mp.rev ? '#fb923c' : '#22d3ee'
      ctx.beginPath(); ctx.arc(cx, cy, 2.5, 0, Math.PI * 2); ctx.fill()
    }
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

  // Mow zones (purple/violet — ordering by zone.order)
  const sortedZones = [...mapData.zones].sort((a, b) => a.order - b.order)
  for (const zone of sortedZones) {
    if (zone.polygon.length < 2) continue
    const isSelected = zone.id === selectedZoneId.value
    ctx.beginPath()
    const [zx0, zy0] = toCanvas(zone.polygon[0][0], zone.polygon[0][1]); ctx.moveTo(zx0, zy0)
    for (let i = 1; i < zone.polygon.length; i++) {
      const [zx, zy] = toCanvas(zone.polygon[i][0], zone.polygon[i][1]); ctx.lineTo(zx, zy)
    }
    ctx.closePath()
    ctx.fillStyle   = isSelected ? 'rgba(168,85,247,0.28)' : 'rgba(168,85,247,0.13)'
    ctx.strokeStyle = isSelected ? '#d8b4fe' : '#a855f7'
    ctx.lineWidth   = isSelected ? 2.5 : 1.5
    ctx.fill(); ctx.stroke()
    ctx.fillStyle = '#a855f7'
    for (const [mx, my] of zone.polygon) {
      const [zx, zy] = toCanvas(mx, my)
      ctx.beginPath(); ctx.arc(zx, zy, 4, 0, Math.PI * 2); ctx.fill()
    }
    // Zone label (order + name) at centroid
    const cx_ = zone.polygon.reduce((s, p) => s + p[0], 0) / zone.polygon.length
    const cy_ = zone.polygon.reduce((s, p) => s + p[1], 0) / zone.polygon.length
    const [lx, ly] = toCanvas(cx_, cy_)
    ctx.fillStyle = isSelected ? '#f3e8ff' : '#d8b4fe'
    ctx.font = '11px sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle'
    ctx.fillText(`${zone.order}. ${zone.settings.name}`, lx, ly)
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
    const color = tool === 'exclusion' ? '#f87171' : tool === 'dockPath' ? '#7dd3fc' : tool === 'zone' ? '#d8b4fe' : '#86efac'
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
  if (ref.kind === 'zone') return mapData.zones[ref.exIdx]?.polygon ?? []
  if (ref.kind === 'perimeter' || ref.exIdx === EX_PERIMETER) return mapData.perimeter
  if (ref.exIdx === EX_DOCK) return mapData.dock
  if (ref.kind === 'dockPath'  || ref.exIdx === EX_DOCKPATH)  return mapData.dockPath
  if (ref.exIdx === EX_OBSTACLE) return mapData.obstacles
  if (ref.exIdx >= 0) return mapData.exclusions[ref.exIdx]
  return mapData.perimeter
}

function syncPerimeterCaptureMeta() {
  if (!mapData.captureMeta) mapData.captureMeta = { perimeter: [] }
  const targetLen = mapData.perimeter.length
  const meta = [...(mapData.captureMeta.perimeter ?? [])]
  if (meta.length > targetLen) meta.length = targetLen
  while (meta.length < targetLen) meta.push(emptyCapturePointMeta())
  mapData.captureMeta.perimeter = meta
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
  mapData.zones.forEach((z, i) => check(z.polygon, 'zone', i))

  return best
}

// ── Load / Save ───────────────────────────────────────────────────────────────
async function loadMap() {
  try {
    const res = await fetch('/api/map')
    if (!res.ok) { status.value = `Laden fehlgeschlagen: HTTP ${res.status}`; return }
    const j = await res.json()
    mapData.perimeter  = j.perimeter  ?? []
    mapData.mow        = (j.mow ?? []).map((e: unknown) =>
      Array.isArray(e)
        ? { p: e as Pt }                                          // legacy [x,y]
        : { p: (e as MowPt).p, rev: (e as MowPt).rev, slow: (e as MowPt).slow }  // new format
    )
    mapData.dock       = j.dock       ?? []
    mapData.dockPath   = j.dockPath   ?? []
    mapData.exclusions = j.exclusions ?? []
    mapData.obstacles  = j.obstacles  ?? []
    mapData.captureMeta = {
      perimeter: j.captureMeta?.perimeter ?? [],
    }
    mapData.zones      = (j.zones ?? []).map((z: Zone, i: number) => ({
      id:       z.id ?? String(i),
      polygon:  z.polygon ?? [],
      order:    z.order   ?? (i + 1),
      settings: {
        name:       z.settings?.name       ?? `Zone ${i + 1}`,
        stripWidth: z.settings?.stripWidth ?? 0.18,
        angle:      z.settings?.angle      ?? 0,
        edgeMowing: z.settings?.edgeMowing ?? true,
        edgeRounds: z.settings?.edgeRounds ?? 1,
        speed:      z.settings?.speed      ?? 1.0,
        pattern:    z.settings?.pattern    ?? 'stripe',
      },
    }))
    selectedZoneId.value = null; editingZoneId.value = null
    if (j.origin) { origin.lat = String(j.origin.lat); origin.lon = String(j.origin.lon) }
    syncPerimeterCaptureMeta()
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
        mow:        mapData.mow.map(mp =>
          mp.rev || mp.slow
            ? { p: mp.p, ...(mp.rev  ? { rev:  true } : {}), ...(mp.slow ? { slow: true } : {}) }
            : mp.p   // compact [x,y] when no flags
        ),
        dock:       derivedDock,
        dockPath:   mapData.dockPath,
        exclusions: mapData.exclusions,
        obstacles:  mapData.obstacles,
        zones:      mapData.zones,
        captureMeta: mapData.captureMeta,
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
    ...mapData.mow.map(mp => mp.p), ...mapData.exclusions.flat(), ...mapData.obstacles,
    ...mapData.zones.flatMap(z => z.polygon),
  ]
  if (allPts.length === 0) { originX = el.width / 2; originY = el.height / 2; scale = 20; return }
  const xs = allPts.map(p => p[0]), ys = allPts.map(p => p[1])
  // BUG-011 fix: avoid spread-overflow with >10k waypoints
  const minX = xs.reduce((a, b) => a < b ? a : b,  Infinity)
  const maxX = xs.reduce((a, b) => a > b ? a : b, -Infinity)
  const minY = ys.reduce((a, b) => a < b ? a : b,  Infinity)
  const maxY = ys.reduce((a, b) => a > b ? a : b, -Infinity)
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
  if ((t === 'dockPath' || t === 'zone') && !perimeterExists.value) {
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

function clearPerimeter()  {
  mapData.perimeter = []
  if (!mapData.captureMeta) mapData.captureMeta = { perimeter: [] }
  mapData.captureMeta.perimeter = []
  dirty.value = true
  draw()
}
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
    syncPerimeterCaptureMeta()
    status.value = 'Perimeter gesetzt.'
  } else if (activeTool.value === 'exclusion') {
    mapData.exclusions = [...mapData.exclusions, [...drawingPts.value]]
    status.value = 'No-Go-Zone hinzugefügt.'
  } else if (activeTool.value === 'zone') {
    const newOrder = mapData.zones.length + 1
    const id       = Date.now().toString()
    mapData.zones.push({
      id,
      polygon:  [...drawingPts.value],
      order:    newOrder,
      settings: { name: `Zone ${newOrder}`, stripWidth: 0.18, angle: 0, edgeMowing: true, edgeRounds: 1, speed: 1.0, pattern: 'stripe' },
    })
    selectedZoneId.value = id
    editingZoneId.value  = id
    status.value = `Mähzone ${newOrder} hinzugefügt.`
  }
  drawingPts.value = []; dirty.value = true; draw()
}

// ── Zone helpers ───────────────────────────────────────────────────────────────

function clearZones() { mapData.zones = []; selectedZoneId.value = null; editingZoneId.value = null; dirty.value = true; draw() }

// ── Mähbahnen-Berechnung ───────────────────────────────────────────────────────

function computePreview() {
  const zone = mapData.zones.find(z => z.id === pathZoneId.value)
  if (!zone) { status.value = 'Bitte eine Mähzone auswählen (Zonen-Panel).'; return }
  if (zone.polygon.length < 3) { status.value = 'Zone hat zu wenig Punkte (mind. 3).'; return }
  // Mähfläche = zone.polygon ∩ mapData.perimeter.
  // Streifen: Scanline clippt zone.polygon, dann Intervall-Schnitt mit Perimeter.
  // Randbahnen: inward-offset Zone (clipped auf Perimeter) +
  //             inward-offset Perimeter (clipped auf Zone) — deckt beide Fälle ab.
  const perimClip = mapData.perimeter.length >= 3
    ? mapData.perimeter as [number,number][]
    : null

  previewMow.value = computeMowPath(
    zone.polygon as [number,number][],
    mapData.exclusions as [number,number][][],
    {
      ...pathSettings,
      stripWidth: zone.settings.stripWidth,
      angle: zone.settings.angle,
      edgeMowing: zone.settings.edgeMowing,
      edgeRounds: zone.settings.edgeRounds,
    },
    perimClip,
  )
  status.value = `Mähpfad für "${zone.settings.name}" berechnet: ${previewMow.value.length} Wegpunkte`
  draw()
}

function saveMowPath() {
  if (previewMow.value.length === 0) return
  mapData.mow = [...previewMow.value]
  dirty.value = true
  status.value = `Mähpfad gespeichert (${mapData.mow.length} Punkte).`
  previewMow.value = []
  draw()
}

function discardPreview() { previewMow.value = []; draw() }

function deleteZone(id: string) {
  mapData.zones = mapData.zones.filter(z => z.id !== id)
  // Renumber order
  mapData.zones.forEach((z, i) => { z.order = i + 1 })
  if (selectedZoneId.value === id) { selectedZoneId.value = null; editingZoneId.value = null }
  dirty.value = true; draw()
}

function moveZone(id: string, dir: -1 | 1) {
  const idx = mapData.zones.findIndex(z => z.id === id)
  if (idx < 0) return
  const swapIdx = idx + dir
  if (swapIdx < 0 || swapIdx >= mapData.zones.length) return
  // Swap order values
  const tmp = mapData.zones[idx].order
  mapData.zones[idx].order    = mapData.zones[swapIdx].order
  mapData.zones[swapIdx].order = tmp
  // Re-sort by order
  mapData.zones.sort((a, b) => a.order - b.order)
  dirty.value = true; draw()
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

  if (ref.kind === 'zone') {
    const zone = mapData.zones[ref.exIdx]
    if (zone) {
      zone.polygon.splice(ref.idx, 1)
      if (zone.polygon.length < 3) {
        mapData.zones.splice(ref.exIdx, 1)
        if (selectedZoneId.value === zone.id) selectedZoneId.value = null
      }
    }
  } else if (ref.exIdx === EX_OBSTACLE) {
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
    if (mapData.captureMeta?.perimeter?.length) {
      mapData.captureMeta.perimeter.splice(ref.idx, 1)
    }
  }
  dirty.value = true; draw()
}

function pointSegmentDistance(cx: number, cy: number, ax: number, ay: number, bx: number, by: number) {
  const abx = bx - ax
  const aby = by - ay
  const abLen2 = abx * abx + aby * aby
  if (abLen2 === 0) {
    const dx = cx - ax
    const dy = cy - ay
    return { dist: Math.sqrt(dx * dx + dy * dy), t: 0 }
  }
  const t = Math.max(0, Math.min(1, ((cx - ax) * abx + (cy - ay) * aby) / abLen2))
  const px = ax + t * abx
  const py = ay + t * aby
  const dx = cx - px
  const dy = cy - py
  return { dist: Math.sqrt(dx * dx + dy * dy), t }
}

function insertIntoPolyline(list: Pt[], insertIdx: number, point: Pt) {
  list.splice(insertIdx, 0, point)
}

function insertOnNearestEdge(cx: number, cy: number) {
  type EdgeRef = { kind: 'perimeter' | 'exclusion' | 'zone' | 'dockPath'; exIdx: number; insertIdx: number; dist: number }
  let best: EdgeRef | null = null

  const checkEdges = (pts: Pt[], kind: EdgeRef['kind'], exIdx: number, closed: boolean) => {
    const segCount = closed ? pts.length : pts.length - 1
    if (segCount < 1) return
    for (let i = 0; i < segCount; i++) {
      const a = pts[i]
      const b = pts[(i + 1) % pts.length]
      const [ax, ay] = toCanvas(a[0], a[1])
      const [bx, by] = toCanvas(b[0], b[1])
      const { dist } = pointSegmentDistance(cx, cy, ax, ay, bx, by)
      if (dist <= SNAP_PX && (!best || dist < best.dist)) {
        best = { kind, exIdx, insertIdx: i + 1, dist }
      }
    }
  }

  checkEdges(mapData.perimeter, 'perimeter', EX_PERIMETER, true)
  checkEdges(mapData.dockPath, 'dockPath', EX_DOCKPATH, false)
  mapData.exclusions.forEach((ex, i) => checkEdges(ex, 'exclusion', i, true))
  mapData.zones.forEach((z, i) => checkEdges(z.polygon, 'zone', i, true))

  if (!best) {
    status.value = 'Keine passende Kante in der Nähe.'
    return
  }

  const edge: EdgeRef = best
  const point = fromCanvas(cx, cy) as Pt
  if (edge.kind === 'perimeter') {
    insertIntoPolyline(mapData.perimeter, edge.insertIdx, point)
    if (!mapData.captureMeta) mapData.captureMeta = { perimeter: [] }
    mapData.captureMeta.perimeter.splice(edge.insertIdx, 0, emptyCapturePointMeta())
    status.value = 'Perimeter-Punkt auf Kante eingefügt.'
  } else if (edge.kind === 'dockPath') {
    insertIntoPolyline(mapData.dockPath, edge.insertIdx, point)
    status.value = 'Dock-Pfad-Punkt auf Kante eingefügt.'
  } else if (edge.kind === 'zone') {
    insertIntoPolyline(mapData.zones[edge.exIdx].polygon, edge.insertIdx, point)
    status.value = 'Zonen-Punkt auf Kante eingefügt.'
  } else {
    insertIntoPolyline(mapData.exclusions[edge.exIdx], edge.insertIdx, point)
    status.value = 'No-Go-Punkt auf Kante eingefügt.'
  }

  dirty.value = true
  draw()
}

// ── Canvas event handlers ──────────────────────────────────────────────────────
function canvasClick(e: MouseEvent) {
  // Suppress click if it was the end of a drag
  if (dragTarget) return
  // Suppress the individual clicks that are part of a double-click (BUG-010 fix):
  // dblclick fires canvasClick twice before canvasDblClick — the second click
  // would add a spurious vertex at the closing position before closePolygon().
  if (e.detail >= 2) return

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
  if (activeTool.value === 'insert') { insertOnNearestEdge(cx, cy); return }

  if (activeTool.value === 'dockPath') {
    drawingPts.value = [...drawingPts.value, [mx, my]]; draw(); return
  }

  if (activeTool.value === 'perimeter' || activeTool.value === 'exclusion' || activeTool.value === 'zone') {
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
  if (activeTool.value === 'perimeter' || activeTool.value === 'exclusion' || activeTool.value === 'zone') {
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

// Auto-select first zone when zones list changes; keep selection valid
watch(() => mapData.zones, (zones) => {
  if (zones.length === 0) { pathZoneId.value = null; return }
  if (!zones.find(z => z.id === pathZoneId.value)) pathZoneId.value = zones[0].id
}, { deep: false })

// Pre-fill preview settings from the selected zone's defaults
watch(pathZoneId, (id) => {
  const zone = mapData.zones.find(z => z.id === id)
  if (!zone) return
  pathSettings.stripWidth = zone.settings.stripWidth
  pathSettings.angle = zone.settings.angle
  pathSettings.edgeMowing = zone.settings.edgeMowing
  pathSettings.edgeRounds = zone.settings.edgeRounds
})
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
          ['zone',      'Mähzone',    'Mähzone zeichnen — Polygon innerhalb des Perimeters'],
          ['dockPath',  'Dock-Pfad',  'Anfahrtsweg zur Dock-Station zeichnen — letzter Punkt = Station · Doppelklick zum Abschließen'],
          ['obstacle',  'Hindernis',  'Simulations-Hindernis setzen (Kreis r=0.5 m)'],
          ['insert',    'Einfügen',   'Neuen Punkt auf die nächste Kante einfügen'],
          ['delete',    'Löschen',    'Nächsten gesetzten Punkt löschen'],
        ] as [Tool, string, string][])"
        :key="tool"
        :title="(tool === 'dockPath' || tool === 'zone') && !perimeterExists ? 'Bitte zuerst Perimeter anlegen' : title"
        :disabled="(tool === 'dockPath' || tool === 'zone') && !perimeterExists"
        @click="selectTool(tool)"
        :class="[
          'px-3 py-1 rounded text-xs font-medium transition-colors',
          (tool === 'dockPath' || tool === 'zone') && !perimeterExists
            ? 'bg-gray-800 text-gray-600 cursor-not-allowed'
            : activeTool === tool
              ? tool === 'perimeter' ? 'bg-green-700 text-green-100'
              : tool === 'exclusion' ? 'bg-red-700 text-red-100'
              : tool === 'zone'      ? 'bg-purple-700 text-purple-100'
              : tool === 'dockPath'  ? 'bg-sky-700 text-sky-100'
              : tool === 'obstacle'  ? 'bg-amber-700 text-amber-100'
              : tool === 'insert'    ? 'bg-cyan-700 text-cyan-100'
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
      <button @click="clearZones" title="Alle Mähzonen löschen" :disabled="mapData.zones.length === 0"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700 disabled:opacity-40">⌫ Mähzonen</button>
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
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm bg-purple-500 opacity-70" /> Mähzone</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-full bg-blue-500 opacity-90" /> Dock-Station (letzter Pfadpunkt)</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm bg-sky-400 opacity-70" /> Dock-Pfad</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-full bg-gray-500 opacity-70" /> Mäh-Pfad</div>
        <div class="flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-full bg-cyan-400 opacity-70" /> Vorschau Mähpfad</div>
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
      <div v-if="activeTool === 'zone'"
        class="absolute top-2 left-2 flex items-center gap-2 text-xs text-gray-400 bg-gray-900 bg-opacity-90 rounded px-2 py-1 select-none">
        <span class="pointer-events-none">Klicken: Punkt setzen &nbsp;·&nbsp; Doppelklick / ⊙ Erstpunkt: schließen &nbsp;·&nbsp; Esc: Abbruch</span>
        <button v-if="isDrawing" @click="undoLastPoint"
          class="pointer-events-auto px-2 py-0.5 rounded bg-gray-700 text-gray-200 hover:bg-gray-600 font-mono"
          title="Letzten Punkt löschen (Backspace)">⌫ Letzten</button>
      </div>
      <div v-else-if="activeTool === 'select'"
        class="absolute top-2 left-2 text-xs text-gray-400 bg-gray-900 bg-opacity-80 rounded px-2 py-1 select-none pointer-events-none">
        Punkt ziehen: verschieben &nbsp;·&nbsp; Freier Bereich ziehen: Karte verschieben
      </div>
    </div>

    <!-- Zones panel -->
    <div v-if="mapData.zones.length > 0 || activeTool === 'zone'"
      class="bg-gray-900 border border-gray-800 rounded-lg overflow-hidden">
      <!-- Panel header -->
      <button
        class="w-full flex items-center gap-2 px-3 py-2 text-xs text-gray-300 hover:bg-gray-800 transition-colors"
        @click="zonesOpen = !zonesOpen"
      >
        <span class="font-semibold text-purple-300">Mähzonen</span>
        <span class="text-gray-500">({{ mapData.zones.length }})</span>
        <span class="ml-auto text-gray-500">{{ zonesOpen ? '▾' : '▸' }}</span>
      </button>

      <!-- Zone list -->
      <div v-if="zonesOpen" class="border-t border-gray-800">
        <div v-if="mapData.zones.length === 0" class="px-3 py-2 text-xs text-gray-600 italic">
          Noch keine Mähzonen — Werkzeug "Mähzone" wählen und Polygon zeichnen.
        </div>
        <div
          v-for="z in [...mapData.zones].sort((a,b) => a.order - b.order)"
          :key="z.id"
          class="px-3 py-2 border-b border-gray-800 last:border-b-0 cursor-pointer hover:bg-gray-800 transition-colors"
          :class="selectedZoneId === z.id ? 'bg-gray-800 border-l-2 border-l-purple-500' : ''"
          @click="selectedZoneId = selectedZoneId === z.id ? null : z.id; editingZoneId = selectedZoneId; draw()"
        >
          <div class="flex items-center gap-2">
            <span class="w-5 h-5 rounded-full bg-purple-800 text-purple-200 text-xs flex items-center justify-center font-bold flex-shrink-0">{{ z.order }}</span>
            <span class="text-xs text-gray-200 font-medium flex-1 min-w-0 truncate">{{ z.settings.name }}</span>
            <span class="text-xs text-gray-500 flex-shrink-0">
              {{ z.settings.pattern === 'stripe' ? '≡' : '◎' }}
              {{ z.settings.stripWidth.toFixed(2) }}m · {{ z.settings.angle }}°
              <template v-if="z.settings.edgeMowing"> · Rand {{ z.settings.edgeRounds }}x</template>
              <template v-else> · ohne Rand</template>
            </span>
            <div class="flex items-center gap-1 flex-shrink-0 ml-1" @click.stop>
              <button @click="moveZone(z.id, -1)" :disabled="z.order === 1"
                class="w-5 h-5 rounded bg-gray-700 hover:bg-gray-600 text-gray-400 text-xs disabled:opacity-30 flex items-center justify-center" title="Nach oben">▲</button>
              <button @click="moveZone(z.id, 1)" :disabled="z.order === mapData.zones.length"
                class="w-5 h-5 rounded bg-gray-700 hover:bg-gray-600 text-gray-400 text-xs disabled:opacity-30 flex items-center justify-center" title="Nach unten">▼</button>
              <button @click="deleteZone(z.id)"
                class="w-5 h-5 rounded bg-gray-700 hover:bg-red-800 text-gray-400 hover:text-red-200 text-xs flex items-center justify-center" title="Zone löschen">✕</button>
            </div>
          </div>

          <template v-if="editingZoneId === z.id">
            <div class="mt-2 ml-7 grid grid-cols-2 gap-2 text-xs" @click.stop>
              <label class="flex flex-col gap-1 text-gray-400">
                <span>Name</span>
                <input
                  v-model="z.settings.name"
                  @input="dirty = true"
                  class="bg-gray-700 border border-purple-600 rounded px-1.5 py-1 text-xs text-gray-100 font-medium"
                  placeholder="Name"
                />
              </label>
              <label class="flex flex-col gap-1 text-gray-400">
                <span>Muster</span>
                <select
                  v-model="z.settings.pattern"
                  @change="dirty = true"
                  class="bg-gray-700 border border-gray-600 rounded px-1.5 py-1 text-xs text-gray-100"
                >
                  <option value="stripe">Streifen</option>
                  <option value="spiral" disabled title="Noch nicht implementiert">Spirale (bald)</option>
                </select>
              </label>
              <label class="flex flex-col gap-1 text-gray-400">
                <span>Breite</span>
                <div class="flex items-center gap-1">
                  <input
                    v-model.number="z.settings.stripWidth"
                    @input="dirty = true"
                    type="number" step="0.01" min="0.05" max="1.0"
                    class="w-20 bg-gray-700 border border-gray-600 rounded px-1.5 py-1 text-xs text-gray-100 font-mono"
                  />
                  <span class="text-gray-600">m</span>
                </div>
              </label>
              <label class="flex flex-col gap-1 text-gray-400">
                <span>Winkel</span>
                <div class="flex items-center gap-1">
                  <input
                    v-model.number="z.settings.angle"
                    @input="dirty = true"
                    type="number" step="1" min="0" max="179"
                    class="w-20 bg-gray-700 border border-gray-600 rounded px-1.5 py-1 text-xs text-gray-100 font-mono"
                  />
                  <span class="text-gray-600">°</span>
                </div>
              </label>
              <label class="flex flex-col gap-1 text-gray-400">
                <span>Geschwindigkeit</span>
                <div class="flex items-center gap-1">
                  <input
                    v-model.number="z.settings.speed"
                    @input="dirty = true"
                    type="number" step="0.1" min="0.1" max="3.0"
                    class="w-20 bg-gray-700 border border-gray-600 rounded px-1.5 py-1 text-xs text-gray-100 font-mono"
                  />
                  <span class="text-gray-600">m/s</span>
                </div>
              </label>
              <label class="flex items-center gap-2 text-gray-300">
                <input
                  v-model="z.settings.edgeMowing"
                  @change="dirty = true"
                  type="checkbox"
                  class="rounded border-gray-600 bg-gray-700 text-purple-500 focus:ring-purple-500"
                />
                <span>Rand mähen</span>
              </label>
              <label class="flex flex-col gap-1 text-gray-400" :class="!z.settings.edgeMowing ? 'opacity-50' : ''">
                <span>Randbahnen</span>
                <div class="flex items-center gap-1">
                  <input
                    v-model.number="z.settings.edgeRounds"
                    @input="dirty = true"
                    :disabled="!z.settings.edgeMowing"
                    type="number" step="1" min="1" max="5"
                    class="w-20 bg-gray-700 border border-gray-600 rounded px-1.5 py-1 text-xs text-gray-100 font-mono disabled:opacity-50"
                  />
                  <span class="text-gray-600">x</span>
                </div>
              </label>
            </div>
          </template>
        </div>
      </div>
    </div>

    <!-- Mähbahnen-Berechnung panel -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg overflow-hidden">
      <button
        class="w-full flex items-center gap-2 px-3 py-2 text-xs text-gray-300 hover:bg-gray-800 transition-colors"
        @click="pathPanelOpen = !pathPanelOpen"
      >
        <span class="font-semibold text-cyan-300">Mähbahnen-Berechnung</span>
        <span v-if="pathZoneId" class="text-purple-400 text-xs">{{ mapData.zones.find(z => z.id === pathZoneId)?.settings.name }}</span>
        <span v-if="previewMow.length > 0" class="text-cyan-500 text-xs">({{ previewMow.length }} Punkte Vorschau)</span>
        <span class="ml-auto text-gray-500">{{ pathPanelOpen ? '▾' : '▸' }}</span>
      </button>
      <div v-if="pathPanelOpen" class="border-t border-gray-800 px-3 py-2 space-y-2">

        <!-- Zone selector -->
        <div v-if="mapData.zones.length === 0" class="text-xs text-amber-500 italic">
          Keine Mähzonen vorhanden — bitte zuerst eine Zone anlegen (Werkzeug "Mähzone").
        </div>
        <div v-else class="flex items-center gap-2 text-xs">
          <label class="text-gray-400 flex-shrink-0">Zone</label>
          <select v-model="pathZoneId"
            class="flex-1 bg-gray-800 border border-purple-700 rounded px-2 py-0.5 text-gray-200 text-xs">
            <option v-for="z in [...mapData.zones].sort((a,b)=>a.order-b.order)" :key="z.id" :value="z.id">
              {{ z.order }}. {{ z.settings.name }} ({{ z.settings.stripWidth.toFixed(2) }}m)
            </option>
          </select>
        </div>

        <!-- Row 1: Angle + Strip width + Overlap -->
        <div class="flex items-center gap-3 flex-wrap text-xs">
          <label class="text-gray-400">Winkel</label>
          <input v-model.number="pathSettings.angle" type="range" min="0" max="179" step="1"
            class="w-24 accent-cyan-500" />
          <span class="text-gray-300 font-mono w-8">{{ pathSettings.angle }}°</span>

          <div class="w-px h-4 bg-gray-700" />

          <label class="text-gray-400">Bahnbreite</label>
          <input v-model.number="pathSettings.stripWidth" type="number" step="0.01" min="0.05" max="1.0"
            class="w-16 bg-gray-800 border border-gray-700 rounded px-1.5 py-0.5 text-gray-200 font-mono text-xs" />
          <span class="text-gray-600">m</span>

          <div class="w-px h-4 bg-gray-700" />

          <label class="text-gray-400">Überlappung</label>
          <input v-model.number="pathSettings.overlap" type="range" min="0" max="50" step="1"
            class="w-16 accent-cyan-500" />
          <span class="text-gray-300 font-mono w-8">{{ pathSettings.overlap }}%</span>
        </div>

        <!-- Row 2: Edge mowing + Turn type + Start side -->
        <div class="flex items-center gap-3 flex-wrap text-xs">
          <label class="flex items-center gap-1.5 text-gray-400 cursor-pointer">
            <input v-model="pathSettings.edgeMowing" type="checkbox" class="accent-cyan-500" />
            Randstreifen
          </label>
          <template v-if="pathSettings.edgeMowing">
            <label class="text-gray-400">Runden</label>
            <input v-model.number="pathSettings.edgeRounds" type="number" min="1" max="5" step="1"
              class="w-12 bg-gray-800 border border-gray-700 rounded px-1.5 py-0.5 text-gray-200 font-mono text-xs" />
          </template>

          <div class="w-px h-4 bg-gray-700" />

          <span class="text-gray-400">Wende:</span>
          <label class="flex items-center gap-1 text-gray-300 cursor-pointer">
            <input v-model="pathSettings.turnType" type="radio" value="kturn" class="accent-cyan-500" /> K-Turn
          </label>
          <label class="flex items-center gap-1 text-gray-300 cursor-pointer">
            <input v-model="pathSettings.turnType" type="radio" value="zeroturn" class="accent-cyan-500" /> Zero-Turn
          </label>

          <div class="w-px h-4 bg-gray-700" />

          <label class="text-gray-400">Start</label>
          <select v-model="pathSettings.startSide"
            class="bg-gray-800 border border-gray-700 rounded px-1.5 py-0.5 text-xs text-gray-200">
            <option value="auto">Auto</option>
            <option value="bottom">Unten</option>
            <option value="top">Oben</option>
            <option value="left">Links</option>
            <option value="right">Rechts</option>
          </select>
        </div>

        <!-- Row 3: Action buttons -->
        <div class="flex items-center gap-2">
          <button @click="computePreview" :disabled="!pathZoneId || mapData.zones.length === 0"
            class="px-3 py-1 rounded text-xs font-medium bg-cyan-800 hover:bg-cyan-700 text-cyan-100 disabled:opacity-40 transition-colors">
            Bahnen berechnen
          </button>
          <button v-if="previewMow.length > 0" @click="saveMowPath"
            class="px-3 py-1 rounded text-xs font-medium bg-green-700 hover:bg-green-600 text-white transition-colors">
            Als Mähpfad speichern ({{ previewMow.length }})
          </button>
          <button v-if="previewMow.length > 0" @click="discardPreview"
            class="px-2 py-1 rounded text-xs bg-gray-700 hover:bg-gray-600 text-gray-400 transition-colors">
            Vorschau verwerfen
          </button>
        </div>

      </div>
    </div>

    <!-- Status bar -->
    <div class="text-xs text-gray-500 px-1">
      {{ status || 'Bereit.' }}
      &nbsp;·&nbsp; Perimeter: {{ mapData.perimeter.length }} Punkte
      &nbsp;·&nbsp; No-Go: {{ mapData.exclusions.length }} Zone(n)
      &nbsp;·&nbsp; Mähzonen: {{ mapData.zones.length }}
      &nbsp;·&nbsp; Dock-Pfad: {{ mapData.dockPath.length > 0 ? mapData.dockPath.length + ' Wegpunkte (letzer = Station)' : '—' }}
      &nbsp;·&nbsp; Mäh-Pfad: {{ mapData.mow.length }}
      <template v-if="previewMow.length > 0">&nbsp;·&nbsp; <span class="text-cyan-600">Vorschau: {{ previewMow.length }}</span></template>
      &nbsp;·&nbsp; Hindernisse: {{ mapData.obstacles.length }}
    </div>

  </div>
</template>
