<script setup lang="ts">
import { ref, reactive, onMounted, onUnmounted, watch } from 'vue'

// ── Types ──────────────────────────────────────────────────────────────────────
type Pt = [number, number]  // [x, y] in local metres (east=+x, north=+y)

interface MapData {
  perimeter:  Pt[]
  mow:        Pt[]
  dock:       Pt[]
  exclusions: Pt[][]
  obstacles:  Pt[]   // simulation obstacle centres (stored in map JSON)
}

type Tool = 'select' | 'perimeter' | 'exclusion' | 'dock' | 'delete' | 'obstacle'

const OBSTACLE_RADIUS_M = 0.5  // visual + collision radius sent to SimulationDriver

// ── State ──────────────────────────────────────────────────────────────────────
const canvas = ref<HTMLCanvasElement | null>(null)

const mapData = reactive<MapData>({ perimeter: [], mow: [], dock: [], exclusions: [], obstacles: [] })
const activeTool = ref<Tool>('select')
const drawingPts = ref<Pt[]>([])          // polygon currently being drawn
const mouseLocal = ref<Pt>([0, 0])        // cursor in local metres
const status     = ref('')
const dirty      = ref(false)             // unsaved changes?

// ── Canvas viewport ────────────────────────────────────────────────────────────
let scale   = 20    // px per metre
let originX = 400   // canvas px for local (0,0)
let originY = 300
let panStart: { cx: number; cy: number; ox: number; oy: number } | null = null
const SNAP_DIST_PX = 12   // pixels: snap-to-first-point threshold

// ── Coordinate helpers ─────────────────────────────────────────────────────────
function toCanvas(mx: number, my: number): [number, number] {
  return [originX + mx * scale, originY - my * scale]
}
function fromCanvas(cx: number, cy: number): [number, number] {
  return [(cx - originX) / scale, (originY - cy) / scale]
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
  const gridStep = scale >= 10 ? 5 : 10  // metres per grid line
  const gridPx   = gridStep * scale
  const startX   = originX % gridPx
  const startY   = originY % gridPx
  for (let x = startX; x < W; x += gridPx) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke() }
  for (let y = startY; y < H; y += gridPx) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke() }

  // Origin cross
  const [ox, oy] = toCanvas(0, 0)
  ctx.strokeStyle = '#374151'
  ctx.lineWidth   = 1
  ctx.beginPath(); ctx.moveTo(ox - 10, oy); ctx.lineTo(ox + 10, oy); ctx.stroke()
  ctx.beginPath(); ctx.moveTo(ox, oy - 10); ctx.lineTo(ox, oy + 10); ctx.stroke()

  // Mow points (small gray dots, read-only)
  if (mapData.mow.length > 0) {
    ctx.fillStyle = '#4b5563'
    for (const [mx, my] of mapData.mow) {
      const [cx, cy] = toCanvas(mx, my)
      ctx.beginPath(); ctx.arc(cx, cy, 2, 0, Math.PI * 2); ctx.fill()
    }
  }

  // Exclusion zones (filled red, semi-transparent)
  for (const excl of mapData.exclusions) {
    if (excl.length < 2) continue
    ctx.beginPath()
    const [x0, y0] = toCanvas(excl[0][0], excl[0][1])
    ctx.moveTo(x0, y0)
    for (let i = 1; i < excl.length; i++) {
      const [cx, cy] = toCanvas(excl[i][0], excl[i][1])
      ctx.lineTo(cx, cy)
    }
    ctx.closePath()
    ctx.fillStyle   = 'rgba(239,68,68,0.18)'
    ctx.strokeStyle = '#ef4444'
    ctx.lineWidth   = 1.5
    ctx.fill()
    ctx.stroke()
    // Vertex dots
    ctx.fillStyle = '#ef4444'
    for (const [mx, my] of excl) {
      const [cx, cy] = toCanvas(mx, my)
      ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill()
    }
  }

  // Perimeter (filled green, semi-transparent)
  if (mapData.perimeter.length >= 2) {
    ctx.beginPath()
    const [x0, y0] = toCanvas(mapData.perimeter[0][0], mapData.perimeter[0][1])
    ctx.moveTo(x0, y0)
    for (let i = 1; i < mapData.perimeter.length; i++) {
      const [cx, cy] = toCanvas(mapData.perimeter[i][0], mapData.perimeter[i][1])
      ctx.lineTo(cx, cy)
    }
    ctx.closePath()
    ctx.fillStyle   = 'rgba(34,197,94,0.12)'
    ctx.strokeStyle = '#22c55e'
    ctx.lineWidth   = 2
    ctx.fill()
    ctx.stroke()
    // Vertex dots
    ctx.fillStyle = '#22c55e'
    for (const [mx, my] of mapData.perimeter) {
      const [cx, cy] = toCanvas(mx, my)
      ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill()
    }
  }

  // Dock point
  if (mapData.dock.length > 0) {
    const [dx, dy] = toCanvas(mapData.dock[0][0], mapData.dock[0][1])
    ctx.fillStyle   = '#3b82f6'
    ctx.strokeStyle = '#93c5fd'
    ctx.lineWidth   = 2
    ctx.beginPath(); ctx.arc(dx, dy, 8, 0, Math.PI * 2); ctx.fill(); ctx.stroke()
    ctx.fillStyle = '#fff'
    ctx.font = 'bold 10px sans-serif'
    ctx.textAlign = 'center'
    ctx.textBaseline = 'middle'
    ctx.fillText('D', dx, dy)
  }

  // Obstacles (amber circles)
  for (const [mx, my] of mapData.obstacles) {
    const [cx, cy] = toCanvas(mx, my)
    const rPx = Math.max(4, OBSTACLE_RADIUS_M * scale)
    ctx.beginPath(); ctx.arc(cx, cy, rPx, 0, Math.PI * 2)
    ctx.fillStyle   = 'rgba(217,119,6,0.25)'
    ctx.strokeStyle = '#d97706'
    ctx.lineWidth   = 1.5
    ctx.fill(); ctx.stroke()
    // Cross marker
    ctx.strokeStyle = '#fbbf24'
    ctx.lineWidth   = 1
    ctx.beginPath(); ctx.moveTo(cx - 5, cy); ctx.lineTo(cx + 5, cy); ctx.stroke()
    ctx.beginPath(); ctx.moveTo(cx, cy - 5); ctx.lineTo(cx, cy + 5); ctx.stroke()
  }

  // Currently drawing polygon (dashed preview)
  if (drawingPts.value.length > 0) {
    const color = activeTool.value === 'exclusion' ? '#f87171' : '#86efac'
    ctx.strokeStyle = color
    ctx.lineWidth   = 1.5
    ctx.setLineDash([5, 4])
    ctx.beginPath()
    const [fx, fy] = toCanvas(drawingPts.value[0][0], drawingPts.value[0][1])
    ctx.moveTo(fx, fy)
    for (let i = 1; i < drawingPts.value.length; i++) {
      const [cx, cy] = toCanvas(drawingPts.value[i][0], drawingPts.value[i][1])
      ctx.lineTo(cx, cy)
    }
    // Line to cursor
    const [mx, my] = mouseLocal.value
    const [mcx, mcy] = toCanvas(mx, my)
    ctx.lineTo(mcx, mcy)
    ctx.stroke()
    ctx.setLineDash([])

    // Drawn vertices
    ctx.fillStyle = color
    for (const [ptx, pty] of drawingPts.value) {
      const [cx, cy] = toCanvas(ptx, pty)
      ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2); ctx.fill()
    }
    // First-point snap ring
    const [s0x, s0y] = toCanvas(drawingPts.value[0][0], drawingPts.value[0][1])
    ctx.strokeStyle = color
    ctx.lineWidth   = 1.5
    ctx.beginPath(); ctx.arc(s0x, s0y, SNAP_DIST_PX, 0, Math.PI * 2); ctx.stroke()
  }
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
    mapData.exclusions = j.exclusions ?? []
    mapData.obstacles  = j.obstacles  ?? []
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
    const res = await fetch('/api/map', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify({
        perimeter:  mapData.perimeter,
        mow:        mapData.mow,
        dock:       mapData.dock,
        exclusions: mapData.exclusions,
        obstacles:  mapData.obstacles,
      }),
    })
    const j = await res.json()
    if (j.ok) {
      dirty.value  = false
      status.value = 'Karte gespeichert.'
    } else {
      status.value = `Speichern fehlgeschlagen: ${j.error ?? 'unbekannt'}`
    }
  } catch (e) {
    status.value = `Speicherfehler: ${e}`
  }
}

// ── Fit view to content ───────────────────────────────────────────────────────
function fitView() {
  const el = canvas.value
  if (!el) return
  const allPts: Pt[] = [
    ...mapData.perimeter,
    ...mapData.dock,
    ...mapData.mow,
    ...mapData.exclusions.flat(),
    ...mapData.obstacles,
  ]
  if (allPts.length === 0) { originX = el.width / 2; originY = el.height / 2; scale = 20; return }
  const xs = allPts.map(p => p[0])
  const ys = allPts.map(p => p[1])
  const minX = Math.min(...xs), maxX = Math.max(...xs)
  const minY = Math.min(...ys), maxY = Math.max(...ys)
  const rangeX = maxX - minX || 10
  const rangeY = maxY - minY || 10
  const margin = 60
  scale   = Math.min((el.width - margin * 2) / rangeX, (el.height - margin * 2) / rangeY)
  scale   = Math.max(2, Math.min(scale, 100))
  originX = el.width  / 2 - ((minX + maxX) / 2) * scale
  originY = el.height / 2 + ((minY + maxY) / 2) * scale
}

// ── Tool actions ──────────────────────────────────────────────────────────────
function selectTool(t: Tool) {
  activeTool.value = t
  drawingPts.value = []
  draw()
}

function clearPerimeter() {
  mapData.perimeter = []
  dirty.value = true
  draw()
}

function clearExclusions() {
  mapData.exclusions = []
  dirty.value = true
  draw()
}

function clearDock() {
  mapData.dock = []
  dirty.value = true
  draw()
}

async function clearObstacles() {
  mapData.obstacles = []
  dirty.value = true
  draw()
  try {
    await fetch('/api/sim/obstacles_clear', { method: 'POST' })
  } catch { /* sim may not be running */ }
  status.value = 'Hindernisse gelöscht.'
}

// Close current drawing polygon and commit it
function closePolygon() {
  if (drawingPts.value.length < 3) { status.value = 'Mindestens 3 Punkte erforderlich.'; return }
  if (activeTool.value === 'perimeter') {
    mapData.perimeter = [...drawingPts.value]
  } else if (activeTool.value === 'exclusion') {
    mapData.exclusions = [...mapData.exclusions, [...drawingPts.value]]
  }
  drawingPts.value = []
  dirty.value = true
  status.value = activeTool.value === 'perimeter' ? 'Perimeter gesetzt.' : 'No-Go-Zone hinzugefügt.'
  draw()
}

// ── Canvas event handlers ──────────────────────────────────────────────────────
function canvasClick(e: MouseEvent) {
  const el = canvas.value!
  const rect = el.getBoundingClientRect()
  const cx   = e.clientX - rect.left
  const cy   = e.clientY - rect.top
  const [mx, my] = fromCanvas(cx, cy)

  if (activeTool.value === 'dock') {
    mapData.dock = [[mx, my]]
    dirty.value  = true
    status.value = `Dock-Punkt gesetzt (${mx.toFixed(1)}, ${my.toFixed(1)}).`
    draw()
    return
  }

  if (activeTool.value === 'obstacle') {
    mapData.obstacles = [...mapData.obstacles, [mx, my]]
    dirty.value  = true
    status.value = `Hindernis gesetzt (${mx.toFixed(1)}, ${my.toFixed(1)}) — r=${OBSTACLE_RADIUS_M} m`
    draw()
    // Fire-and-forget to SimulationDriver (only active in --sim mode)
    fetch('/api/sim/obstacle', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify({ x: mx, y: my, radius: OBSTACLE_RADIUS_M }),
    }).catch(() => { /* sim not running */ })
    return
  }

  if (activeTool.value === 'delete') {
    // Remove nearest vertex
    deleteNearestVertex(cx, cy)
    return
  }

  if (activeTool.value === 'perimeter' || activeTool.value === 'exclusion') {
    // Snap-to-first: close polygon if click near first point
    if (drawingPts.value.length >= 3) {
      const [fx, fy] = toCanvas(drawingPts.value[0][0], drawingPts.value[0][1])
      const dx = cx - fx, dy = cy - fy
      if (Math.sqrt(dx * dx + dy * dy) < SNAP_DIST_PX) {
        closePolygon()
        return
      }
    }
    drawingPts.value = [...drawingPts.value, [mx, my]]
    draw()
  }
}

function canvasDblClick(e: MouseEvent) {
  if (activeTool.value === 'perimeter' || activeTool.value === 'exclusion') {
    e.preventDefault()
    closePolygon()
  }
}

function canvasMouseMove(e: MouseEvent) {
  const el = canvas.value!
  const rect = el.getBoundingClientRect()
  const cx   = e.clientX - rect.left
  const cy   = e.clientY - rect.top
  mouseLocal.value = fromCanvas(cx, cy)

  // Pan (middle-mouse or select tool + drag)
  if (panStart) {
    originX = panStart.ox + (cx - panStart.cx)
    originY = panStart.oy + (cy - panStart.cy)
  }
  draw()
}

function canvasMouseDown(e: MouseEvent) {
  if (e.button === 1 || (e.button === 0 && activeTool.value === 'select')) {
    const el = canvas.value!
    const rect = el.getBoundingClientRect()
    panStart = { cx: e.clientX - rect.left, cy: e.clientY - rect.top, ox: originX, oy: originY }
    e.preventDefault()
  }
}

function canvasMouseUp(e: MouseEvent) {
  if (panStart) {
    const el = canvas.value!
    const rect = el.getBoundingClientRect()
    const movedX = Math.abs(e.clientX - rect.left - panStart.cx)
    const movedY = Math.abs(e.clientY - rect.top  - panStart.cy)
    // If barely moved, treat as a click for select-mode (don't suppress)
    if (movedX < 3 && movedY < 3 && e.button === 0 && activeTool.value === 'select') {
      panStart = null
      return
    }
    panStart = null
  }
}

function canvasWheel(e: WheelEvent) {
  e.preventDefault()
  const el = canvas.value!
  const rect = el.getBoundingClientRect()
  const cx = e.clientX - rect.left
  const cy = e.clientY - rect.top
  const factor = e.deltaY < 0 ? 1.15 : 1 / 1.15
  const newScale = Math.max(1, Math.min(scale * factor, 200))
  originX = cx - (cx - originX) * (newScale / scale)
  originY = cy - (cy - originY) * (newScale / scale)
  scale = newScale
  draw()
}

// ── Delete nearest vertex ─────────────────────────────────────────────────────
function deleteNearestVertex(cx: number, cy: number) {
  const THRESH = 12
  let bestDist = THRESH + 1
  let bestTarget: { list: Pt[]; idx: number; exIdx: number } | null = null

  function checkList(pts: Pt[], exIdx = -1) {
    for (let i = 0; i < pts.length; i++) {
      const [px, py] = toCanvas(pts[i][0], pts[i][1])
      const d = Math.sqrt((cx - px) ** 2 + (cy - py) ** 2)
      if (d < bestDist) { bestDist = d; bestTarget = { list: pts, idx: i, exIdx } }
    }
  }

  checkList(mapData.perimeter)
  if (mapData.dock.length) checkList(mapData.dock)
  mapData.exclusions.forEach((ex, i) => checkList(ex, i))
  checkList(mapData.obstacles, -2)  // -2 = obstacles list

  if (!bestTarget) { status.value = 'Kein Punkt in der Nähe.'; return }
  const t = bestTarget as { list: Pt[]; idx: number; exIdx: number }
  if (t.exIdx === -2) {
    mapData.obstacles.splice(t.idx, 1)
  } else if (t.exIdx >= 0) {
    mapData.exclusions[t.exIdx].splice(t.idx, 1)
    if (mapData.exclusions[t.exIdx].length < 3) mapData.exclusions.splice(t.exIdx, 1)
  } else if (t.list === mapData.dock) {
    mapData.dock = []
  } else {
    mapData.perimeter.splice(t.idx, 1)
  }
  dirty.value = true
  draw()
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

// ── Lifecycle ─────────────────────────────────────────────────────────────────
onMounted(async () => {
  setupResize()
  await loadMap()
})
onUnmounted(() => resizeObs?.disconnect())
watch(activeTool, draw)
</script>

<template>
  <div class="flex flex-col h-full gap-2">

    <!-- Toolbar -->
    <div class="flex items-center gap-2 flex-wrap bg-gray-900 border border-gray-800 rounded-lg p-2">

      <!-- Tool buttons -->
      <span class="text-xs text-gray-500 mr-1">Werkzeug:</span>
      <button
        v-for="[tool, label, title] in ([
          ['select',    'Zeiger',    'Verschieben / Pan'],
          ['perimeter', 'Perimeter', 'Perimeter zeichnen — Doppelklick oder Klick auf ersten Punkt zum Schließen'],
          ['exclusion', 'No-Go',     'No-Go-Zone zeichnen'],
          ['dock',      'Dock',      'Dock-Punkt setzen'],
          ['obstacle',  'Hindernis', 'Simulations-Hindernis setzen (Kreis, r=0.5 m) — sendet an SimulationDriver'],
          ['delete',    'Löschen',   'Nächsten Punkt löschen'],
        ] as [Tool, string, string][])"
        :key="tool"
        :title="title"
        @click="selectTool(tool)"
        :class="[
          'px-3 py-1 rounded text-xs font-medium transition-colors',
          activeTool === tool
            ? tool === 'perimeter' ? 'bg-green-700 text-green-100'
            : tool === 'exclusion' ? 'bg-red-700 text-red-100'
            : tool === 'dock'      ? 'bg-blue-700 text-blue-100'
            : tool === 'obstacle'  ? 'bg-amber-700 text-amber-100'
            : tool === 'delete'    ? 'bg-orange-700 text-orange-100'
            :                        'bg-gray-600 text-gray-100'
            : 'bg-gray-800 text-gray-300 hover:bg-gray-700'
        ]"
      >{{ label }}</button>

      <div class="w-px h-5 bg-gray-700 mx-1" />

      <!-- Clear buttons -->
      <button @click="clearPerimeter"   title="Perimeter löschen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ Perimeter</button>
      <button @click="clearExclusions"  title="Alle No-Go-Zonen löschen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ No-Go</button>
      <button @click="clearDock"        title="Dock-Punkt löschen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ Dock</button>
      <button @click="clearObstacles"   title="Alle Hindernisse löschen + SimulationDriver reset"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⌫ Hindernisse</button>

      <div class="w-px h-5 bg-gray-700 mx-1" />

      <!-- View -->
      <button @click="fitView(); draw()" title="Ansicht anpassen"
        class="px-2 py-1 rounded text-xs bg-gray-800 text-gray-400 hover:bg-gray-700">⊡ Fit</button>

      <div class="flex-1" />

      <!-- Save / Load -->
      <button @click="loadMap"
        class="px-3 py-1 rounded text-xs bg-gray-700 text-gray-200 hover:bg-gray-600">Laden</button>
      <button @click="saveMap"
        :class="['px-3 py-1 rounded text-xs font-medium transition-colors',
          dirty ? 'bg-green-700 hover:bg-green-600 text-white' : 'bg-gray-700 text-gray-400']">
        {{ dirty ? 'Speichern *' : 'Gespeichert' }}
      </button>
    </div>

    <!-- Canvas area -->
    <div class="relative flex-1 min-h-0 bg-gray-950 border border-gray-800 rounded-lg overflow-hidden">
      <canvas
        ref="canvas"
        class="w-full h-full block cursor-crosshair"
        :style="activeTool === 'select' ? 'cursor:grab' : activeTool === 'delete' ? 'cursor:not-allowed' : 'cursor:crosshair'"
        @click="canvasClick"
        @dblclick="canvasDblClick"
        @mousemove="canvasMouseMove"
        @mousedown="canvasMouseDown"
        @mouseup="canvasMouseUp"
        @wheel.prevent="canvasWheel"
        @contextmenu.prevent
      />

      <!-- Cursor coordinates overlay -->
      <div class="absolute bottom-2 left-2 text-xs text-gray-500 select-none pointer-events-none font-mono">
        x={{ mouseLocal[0].toFixed(2) }} m &nbsp; y={{ mouseLocal[1].toFixed(2) }} m
      </div>

      <!-- Legend -->
      <div class="absolute top-2 right-2 text-xs text-gray-500 select-none pointer-events-none space-y-1">
        <div class="flex items-center gap-1">
          <span class="inline-block w-3 h-3 rounded-sm bg-green-500 opacity-70" /> Perimeter
        </div>
        <div class="flex items-center gap-1">
          <span class="inline-block w-3 h-3 rounded-sm bg-red-500 opacity-70" /> No-Go
        </div>
        <div class="flex items-center gap-1">
          <span class="inline-block w-3 h-3 rounded-full bg-blue-500 opacity-90" /> Dock
        </div>
        <div class="flex items-center gap-1">
          <span class="inline-block w-3 h-3 rounded-full bg-gray-500 opacity-70" /> Mäh-Pfad
        </div>
        <div class="flex items-center gap-1">
          <span class="inline-block w-3 h-3 rounded-full bg-amber-500 opacity-70" /> Hindernis (Sim)
        </div>
      </div>

      <!-- Drawing hint -->
      <div v-if="activeTool === 'perimeter' || activeTool === 'exclusion'"
        class="absolute top-2 left-2 text-xs text-gray-400 bg-gray-900 bg-opacity-80 rounded px-2 py-1 select-none pointer-events-none">
        Klicken: Punkt setzen &nbsp;·&nbsp; Doppelklick oder ⊙ erstes Punkt: Polygon schließen
      </div>
    </div>

    <!-- Status bar -->
    <div class="text-xs text-gray-500 px-1">
      {{ status || 'Bereit.' }}
      &nbsp;·&nbsp; Perimeter: {{ mapData.perimeter.length }} Punkte
      &nbsp;·&nbsp; No-Go: {{ mapData.exclusions.length }} Zone(n)
      &nbsp;·&nbsp; Dock: {{ mapData.dock.length ? 'gesetzt' : '—' }}
      &nbsp;·&nbsp; Mäh-Pfad: {{ mapData.mow.length }} Wegpunkte (nur-lesen)
      &nbsp;·&nbsp; Hindernisse: {{ mapData.obstacles.length }}
    </div>

  </div>
</template>
