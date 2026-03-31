<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'

const emit = defineEmits<{ close: [] }>()
const { sendCmd, telemetry } = useTelemetry()

// PAD must match the CSS width/height of .jpad exactly
const PAD  = 176
const HALF = PAD / 2
const R    = 64   // max knob travel radius px

const padEl   = ref<HTMLDivElement | null>(null)
const panelEl = ref<HTMLDivElement | null>(null)
const knobX   = ref(HALF)
const knobY   = ref(HALF)

// ── Panel drag position ───────────────────────────────────────────────────────
// null = use CSS default bottom/right
const panelX = ref<number | null>(null)
const panelY = ref<number | null>(null)
let panDragging = false
let panStart = { mx: 0, my: 0, ox: 0, oy: 0 }

const panelStyle = computed(() =>
  panelX.value !== null && panelY.value !== null
    ? { position: 'fixed' as const, left: panelX.value + 'px', top: panelY.value + 'px', zIndex: 200 }
    : { position: 'fixed' as const, bottom: '16px', right: '272px', zIndex: 200 }
)

function onHeaderMouseDown(e: MouseEvent) {
  e.preventDefault()
  const rect = panelEl.value!.getBoundingClientRect()
  if (panelX.value === null) { panelX.value = rect.left; panelY.value = rect.top }
  panStart = { mx: e.clientX, my: e.clientY, ox: panelX.value!, oy: panelY.value! }
  panDragging = true
  window.addEventListener('mousemove', onPanMove)
  window.addEventListener('mouseup', onPanUp, { once: true })
}

function onPanMove(e: MouseEvent) {
  if (!panDragging) return
  panelX.value = panStart.ox + (e.clientX - panStart.mx)
  panelY.value = panStart.oy + (e.clientY - panStart.my)
}

function onPanUp() {
  panDragging = false
  window.removeEventListener('mousemove', onPanMove)
}

// ── Joystick logic ────────────────────────────────────────────────────────────
let active = false
let timer: ReturnType<typeof setInterval> | null = null

const linear  = computed(() => -(knobY.value - HALF) / R)
const angular = computed(() =>  (knobX.value - HALF) / R)

const notIdle = computed(() => telemetry.value.op !== 'Idle')

function clamp(v: number) { return Math.max(-1, Math.min(1, v)) }

function sendDrive() {
  sendCmd('drive', {
    linear:  parseFloat(clamp(linear.value).toFixed(2)),
    angular: parseFloat(clamp(angular.value).toFixed(2)),
  })
}

function moveKnob(cx: number, cy: number) {
  const dx = cx - HALF, dy = cy - HALF
  const dist = Math.sqrt(dx * dx + dy * dy)
  const scale = dist > R ? R / dist : 1
  knobX.value = HALF + dx * scale
  knobY.value = HALF + dy * scale
}

function startTracking(cx: number, cy: number) {
  active = true
  moveKnob(cx, cy)
  if (!timer) timer = setInterval(sendDrive, 80)
}

function stopTracking() {
  if (!active) return
  active = false
  knobX.value = HALF
  knobY.value = HALF
  if (timer) { clearInterval(timer); timer = null }
  sendCmd('drive', { linear: 0, angular: 0 })
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
function onMouseDown(e: MouseEvent) {
  const rect = padEl.value!.getBoundingClientRect()
  startTracking(e.clientX - rect.left, e.clientY - rect.top)
  window.addEventListener('mousemove', onMouseMove)
  window.addEventListener('mouseup', onMouseUp, { once: true })
}

function onMouseMove(e: MouseEvent) {
  if (!active) return
  const rect = padEl.value!.getBoundingClientRect()
  moveKnob(e.clientX - rect.left, e.clientY - rect.top)
}

function onMouseUp() {
  stopTracking()
  window.removeEventListener('mousemove', onMouseMove)
}

// ── Touch ─────────────────────────────────────────────────────────────────────
function onTouchStart(e: TouchEvent) {
  e.preventDefault()
  const rect = padEl.value!.getBoundingClientRect()
  const t = e.touches[0]
  startTracking(t.clientX - rect.left, t.clientY - rect.top)
}

function onTouchMove(e: TouchEvent) {
  e.preventDefault()
  if (!active) return
  const rect = padEl.value!.getBoundingClientRect()
  const t = e.touches[0]
  moveKnob(t.clientX - rect.left, t.clientY - rect.top)
}

onMounted(() => {
  // ensure initial knob is truly centered after render
  knobX.value = HALF
  knobY.value = HALF
})

onUnmounted(() => {
  stopTracking()
  window.removeEventListener('mousemove', onMouseMove)
  window.removeEventListener('mousemove', onPanMove)
})
</script>

<template>
  <div ref="panelEl" class="jw" :style="panelStyle">

    <!-- Header / drag handle -->
    <div class="jw-head" @mousedown.prevent="onHeaderMouseDown">
      <span class="jw-drag">⠿</span>
      <span class="jw-title">Joystick</span>
      <button class="jw-close" @click.stop="$emit('close')" @mousedown.stop>✕</button>
    </div>

    <!-- Warning when not idle -->
    <div v-if="notIdle" class="jw-warn">Nur im Stillstand verfügbar</div>

    <!-- Pad -->
    <div
      ref="padEl"
      class="jpad"
      :class="{ 'jpad-disabled': notIdle }"
      @mousedown.prevent="!notIdle && onMouseDown($event)"
      @touchstart.prevent="!notIdle && onTouchStart($event)"
      @touchmove.prevent="onTouchMove($event)"
      @touchend.prevent="stopTracking()"
    >
      <div class="jcross-h"></div>
      <div class="jcross-v"></div>
      <div class="jknob" :style="{ left: knobX - 22 + 'px', top: knobY - 22 + 'px' }"></div>
      <div class="jlbl jlbl-top">▲</div>
      <div class="jlbl jlbl-bot">▼</div>
      <div class="jlbl jlbl-lft">◀</div>
      <div class="jlbl jlbl-rgt">▶</div>
    </div>

    <!-- Readout -->
    <div class="jread">
      <div class="jread-row">
        <span class="jread-lbl">Vorwärts</span>
        <span class="jread-val" :class="linear > 0.05 ? 'pos' : linear < -0.05 ? 'neg' : 'zero'">
          {{ (clamp(linear) * 100).toFixed(0) }}%
        </span>
      </div>
      <div class="jread-row">
        <span class="jread-lbl">Drehung</span>
        <span class="jread-val" :class="Math.abs(angular) > 0.05 ? 'pos' : 'zero'">
          {{ (clamp(angular) * 100).toFixed(0) }}%
        </span>
      </div>
    </div>
  </div>
</template>

<style scoped>
*, *::before, *::after { box-sizing: border-box; }

.jw {
  width: 200px;
  background: #0a1020 !important;
  border: 1px solid #1e3a5f;
  border-radius: 12px;
  padding: 0;
  display: flex;
  flex-direction: column;
  box-shadow: 0 8px 32px rgba(0,0,0,0.7);
  overflow: hidden;
}

/* Header (drag handle) */
.jw-head {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 10px 8px 8px;
  background: #0f1829 !important;
  border-bottom: 1px solid #1e3a5f;
  cursor: move;
  user-select: none;
}
.jw-drag  { color: #334155 !important; font-size: 14px; flex-shrink: 0; }
.jw-title { font-size: 11px; font-weight: 500; color: #60a5fa !important; text-transform: uppercase; letter-spacing: 1px; flex: 1; }
.jw-close {
  background: none; border: none; color: #475569 !important; cursor: pointer;
  font-size: 12px; padding: 1px 4px; border-radius: 4px; flex-shrink: 0;
}
.jw-close:hover { background: #1e3a5f !important; color: #94a3b8 !important; }

.jw-warn {
  font-size: 10px; color: #fbbf24 !important; background: #1c1200 !important;
  border-bottom: 1px solid #92400e; padding: 5px 10px; text-align: center;
}

/* ── Pad ── */
.jpad {
  width: 176px;
  height: 176px;
  border-radius: 50%;
  background: #060c17 !important;
  border: 2px solid #1e3a5f;
  position: relative;
  overflow: hidden;
  cursor: grab;
  flex-shrink: 0;
  align-self: center;
  margin: 12px auto;
  touch-action: none;
  user-select: none;
}
.jpad:active { cursor: grabbing; }
.jpad-disabled { opacity: 0.35; cursor: not-allowed; }

.jcross-h {
  position: absolute; top: 50%; left: 8%; width: 84%; height: 1px;
  background: #1e3a5f; transform: translateY(-50%); pointer-events: none;
}
.jcross-v {
  position: absolute; left: 50%; top: 8%; height: 84%; width: 1px;
  background: #1e3a5f; transform: translateX(-50%); pointer-events: none;
}

.jknob {
  position: absolute;
  width: 44px;
  height: 44px;
  border-radius: 50%;
  background: radial-gradient(circle at 38% 38%, #3b82f6, #1e40af) !important;
  border: 2px solid #60a5fa;
  box-shadow: 0 0 12px rgba(59,130,246,0.5);
  pointer-events: none;
}

.jlbl {
  position: absolute;
  font-size: 10px;
  color: #1e3a5f !important;
  pointer-events: none;
}
.jlbl-top { top: 6px;    left: 50%; transform: translateX(-50%); }
.jlbl-bot { bottom: 6px; left: 50%; transform: translateX(-50%); }
.jlbl-lft { left: 8px;   top:  50%; transform: translateY(-50%); }
.jlbl-rgt { right: 8px;  top:  50%; transform: translateY(-50%); }

/* ── Readout ── */
.jread { display: flex; flex-direction: column; gap: 4px; padding: 0 12px 10px; }
.jread-row { display: flex; justify-content: space-between; align-items: center; }
.jread-lbl { font-size: 10px; color: #475569 !important; }
.jread-val { font-size: 12px; font-family: monospace; font-weight: 500; min-width: 40px; text-align: right; }
.jread-val.pos  { color: #4ade80 !important; }
.jread-val.neg  { color: #f87171 !important; }
.jread-val.zero { color: #334155 !important; }
</style>
