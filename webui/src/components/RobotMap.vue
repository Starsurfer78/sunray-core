<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted } from 'vue'

const props = defineProps<{
  x:       number   // metres east
  y:       number   // metres north
  heading: number   // radians, 0 = east
  op:      string
}>()

const canvas  = ref<HTMLCanvasElement | null>(null)
const scale   = ref(20)   // px per metre
const originX = ref(0)    // canvas-pixel position of local origin
const originY = ref(0)

// ── Coordinate transform: local metres → canvas pixels ───────────────────────
// Local: +x = east, +y = north
// Canvas: +x = right, +y = down → flip Y

function toCanvas(mx: number, my: number): [number, number] {
  return [
    originX.value + mx * scale.value,
    originY.value - my * scale.value,
  ]
}

// ── Rounded rectangle helper ──────────────────────────────────────────────────

function roundedRect(
  ctx: CanvasRenderingContext2D,
  x: number, y: number, w: number, h: number, r: number,
) {
  ctx.beginPath()
  ctx.moveTo(x + r, y)
  ctx.lineTo(x + w - r, y)
  ctx.arcTo(x + w, y,     x + w, y + r,     r)
  ctx.lineTo(x + w, y + h - r)
  ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
  ctx.lineTo(x + r, y + h)
  ctx.arcTo(x,     y + h, x,     y + h - r, r)
  ctx.lineTo(x,     y + r)
  ctx.arcTo(x,     y,     x + r, y,         r)
  ctx.closePath()
}

// ── Draw ──────────────────────────────────────────────────────────────────────

function draw() {
  const c = canvas.value
  if (!c) return
  const ctx = c.getContext('2d')!

  // Background
  ctx.fillStyle = '#070d18'
  ctx.fillRect(0, 0, c.width, c.height)

  drawGrid(ctx, c.width, c.height)
  drawDock(ctx)
  drawRobot(ctx, props.x, props.y, props.heading)
}

function drawGrid(ctx: CanvasRenderingContext2D, w: number, h: number) {
  ctx.strokeStyle = '#1e3a5f'
  ctx.lineWidth = 0.5
  const step = scale.value
  const ox = originX.value % step
  const oy = originY.value % step

  for (let px = ox; px < w; px += step) {
    ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, h); ctx.stroke()
  }
  for (let py = oy; py < h; py += step) {
    ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(w, py); ctx.stroke()
  }

  // Origin cross (slightly brighter)
  const [ox2, oy2] = toCanvas(0, 0)
  ctx.strokeStyle = '#2d5a8e'
  ctx.lineWidth = 1
  ctx.beginPath(); ctx.moveTo(ox2 - 10, oy2); ctx.lineTo(ox2 + 10, oy2); ctx.stroke()
  ctx.beginPath(); ctx.moveTo(ox2, oy2 - 10); ctx.lineTo(ox2, oy2 + 10); ctx.stroke()
}

function drawDock(ctx: CanvasRenderingContext2D) {
  const [cx, cy] = toCanvas(0, 0)
  const w = 28, h = 24

  // Amber rect
  roundedRect(ctx, cx - w / 2, cy - h / 2, w, h, 4)
  ctx.fillStyle = '#d97706'
  ctx.fill()

  // "D" label
  ctx.fillStyle = '#0a0f1a'
  ctx.font = 'bold 12px sans-serif'
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.fillText('D', cx, cy)
}

function drawRobot(
  ctx: CanvasRenderingContext2D,
  mx: number, my: number,
  heading: number,
) {
  const [cx, cy] = toCanvas(mx, my)
  const r = Math.max(10, scale.value * 0.35)

  ctx.save()
  ctx.translate(cx, cy)
  // heading 0 = east (+x canvas); increases counter-clockwise in local frame
  ctx.rotate(-heading)

  // Body circle: fill #0f1829, stroke #3b82f6
  ctx.beginPath()
  ctx.arc(0, 0, r, 0, Math.PI * 2)
  ctx.fillStyle = '#0f1829'
  ctx.fill()
  ctx.strokeStyle = '#3b82f6'
  ctx.lineWidth = 2
  ctx.stroke()

  // Center dot (ratio 4.5/12 ≈ 0.375 from reference)
  ctx.beginPath()
  ctx.arc(0, 0, r * 0.375, 0, Math.PI * 2)
  ctx.fillStyle = '#60a5fa'
  ctx.fill()

  // Direction arrow: line from body edge, then arrowhead
  // Reference proportions (r=12): line ends at 2.17r, tip at 2.5r, base half-width 0.42r
  const lineEnd  = r * 2.17
  const tipX     = r * 2.5
  const baseHalf = r * 0.42

  ctx.beginPath()
  ctx.moveTo(r, 0)
  ctx.lineTo(lineEnd, 0)
  ctx.strokeStyle = '#60a5fa'
  ctx.lineWidth = 2.5
  ctx.lineCap = 'round'
  ctx.stroke()

  ctx.beginPath()
  ctx.moveTo(tipX, 0)
  ctx.lineTo(lineEnd - r * 0.1, baseHalf)
  ctx.lineTo(lineEnd - r * 0.1, -baseHalf)
  ctx.closePath()
  ctx.fillStyle = '#60a5fa'
  ctx.fill()

  ctx.restore()
}

// ── Resize ────────────────────────────────────────────────────────────────────

function resize() {
  const c = canvas.value
  if (!c) return
  const rect = c.parentElement!.getBoundingClientRect()
  c.width  = Math.floor(rect.width)
  c.height = Math.floor(rect.height)
  originX.value = c.width  / 2
  originY.value = c.height / 2
  draw()
}

// ── Zoom / pan (public, called from Dashboard) ───────────────────────────────

function zoomIn() {
  scale.value = Math.min(100, scale.value * 1.2)
  draw()
}

function zoomOut() {
  scale.value = Math.max(3, scale.value / 1.2)
  draw()
}

function centerRobot() {
  const c = canvas.value
  if (!c) return
  const [px, py] = toCanvas(props.x, props.y)
  originX.value += c.width  / 2 - px
  originY.value += c.height / 2 - py
  draw()
}

defineExpose({ zoomIn, zoomOut, centerRobot })

// ── Mouse/touch pan ───────────────────────────────────────────────────────────

let dragging = false, lastPx = 0, lastPy = 0

function onPointerDown(e: PointerEvent) {
  dragging = true; lastPx = e.clientX; lastPy = e.clientY
  ;(e.target as HTMLElement).setPointerCapture(e.pointerId)
}
function onPointerMove(e: PointerEvent) {
  if (!dragging) return
  originX.value += e.clientX - lastPx
  originY.value += e.clientY - lastPy
  lastPx = e.clientX; lastPy = e.clientY
  draw()
}
function onPointerUp() { dragging = false }

function onWheel(e: WheelEvent) {
  e.preventDefault()
  const factor = e.deltaY < 0 ? 1.15 : 0.87
  scale.value = Math.max(3, Math.min(100, scale.value * factor))
  draw()
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

const ro = new ResizeObserver(resize)

onMounted(() => {
  if (canvas.value?.parentElement) ro.observe(canvas.value.parentElement)
  resize()
})
onUnmounted(() => ro.disconnect())

watch([() => props.x, () => props.y, () => props.heading, () => props.op], draw)
</script>

<template>
  <div class="map-wrap">
    <canvas
      ref="canvas"
      class="map-canvas"
      @pointerdown="onPointerDown"
      @pointermove="onPointerMove"
      @pointerup="onPointerUp"
      @wheel.prevent="onWheel"
    />
  </div>
</template>

<style scoped>
.map-wrap {
  width: 100%;
  height: 100%;
  overflow: hidden;
  user-select: none;
}
.map-canvas {
  display: block;
  cursor: grab;
}
.map-canvas:active {
  cursor: grabbing;
}
</style>
