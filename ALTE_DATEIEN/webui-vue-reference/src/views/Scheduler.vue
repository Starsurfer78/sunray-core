<script setup lang="ts">
import { ref, onMounted } from 'vue'

// ── Types ─────────────────────────────────────────────────────────────────────

interface ScheduleEntry {
  wday:           number   // 0=Sun … 6=Sat
  start_hhmm:     number   // e.g. 800
  end_hhmm:       number   // e.g. 1200
  enabled:        boolean
  rain_delay_min: number
}

const WDAY_NAMES = ['So', 'Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa']
const WDAY_FULL  = ['Sonntag', 'Montag', 'Dienstag', 'Mittwoch', 'Donnerstag', 'Freitag', 'Samstag']

// ── State ─────────────────────────────────────────────────────────────────────

const entries   = ref<ScheduleEntry[]>([])
const loading   = ref(true)
const saving    = ref(false)
const toast     = ref<{ msg: string; ok: boolean } | null>(null)
let   toastTimer: ReturnType<typeof setTimeout> | null = null

function showToast(msg: string, ok = true) {
  toast.value = { msg, ok }
  if (toastTimer) clearTimeout(toastTimer)
  toastTimer = setTimeout(() => { toast.value = null }, 3000)
}

// ── API ───────────────────────────────────────────────────────────────────────

async function load() {
  loading.value = true
  try {
    const r = await fetch('/api/schedule')
    entries.value = await r.json() as ScheduleEntry[]
  } catch {
    showToast('Laden fehlgeschlagen', false)
  } finally {
    loading.value = false
  }
}

async function save() {
  saving.value = true
  try {
    const r = await fetch('/api/schedule', {
      method:  'PUT',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(entries.value),
    })
    const j = await r.json()
    if (j.ok) showToast('Gespeichert')
    else      showToast('Speichern fehlgeschlagen', false)
  } catch {
    showToast('Verbindungsfehler', false)
  } finally {
    saving.value = false
  }
}

onMounted(load)

// ── Helpers ───────────────────────────────────────────────────────────────────

function hhmm2str(hhmm: number): string {
  const h = Math.floor(hhmm / 100).toString().padStart(2, '0')
  const m = (hhmm % 100).toString().padStart(2, '0')
  return `${h}:${m}`
}

function str2hhmm(s: string): number {
  const [h, m] = s.split(':').map(Number)
  return (h || 0) * 100 + (m || 0)
}

function addEntry() {
  entries.value.push({ wday: 1, start_hhmm: 800, end_hhmm: 1200, enabled: true, rain_delay_min: 0 })
}

function removeEntry(i: number) {
  entries.value.splice(i, 1)
}

function onStartChange(e: ScheduleEntry, val: string) {
  e.start_hhmm = str2hhmm(val)
}
function onEndChange(e: ScheduleEntry, val: string) {
  e.end_hhmm = str2hhmm(val)
}
</script>

<template>
  <div class="sc-wrap">
    <div class="sc-header">
      <h2 class="sc-title">Zeitplan</h2>
      <div class="sc-header-actions">
        <button class="sc-btn sc-btn-add" @click="addEntry">+ Eintrag</button>
        <button class="sc-btn sc-btn-save" :disabled="saving" @click="save">
          {{ saving ? 'Speichern…' : 'Speichern' }}
        </button>
      </div>
    </div>

    <!-- Toast -->
    <Transition name="toast">
      <div v-if="toast" class="sc-toast" :class="toast.ok ? 'sc-toast-ok' : 'sc-toast-err'">
        {{ toast.msg }}
      </div>
    </Transition>

    <!-- Loading -->
    <div v-if="loading" class="sc-empty">Lade…</div>

    <!-- Empty -->
    <div v-else-if="entries.length === 0" class="sc-empty">
      <div class="sc-empty-icon">🕐</div>
      <p>Kein Zeitplan konfiguriert.</p>
      <button class="sc-btn sc-btn-add" @click="addEntry">+ Ersten Eintrag hinzufügen</button>
    </div>

    <!-- Entry list -->
    <div v-else class="sc-list">
      <div
        v-for="(e, i) in entries"
        :key="i"
        class="sc-entry"
        :class="{ 'sc-entry-disabled': !e.enabled }">

        <!-- Toggle enabled -->
        <button
          class="sc-toggle"
          :class="e.enabled ? 'sc-toggle-on' : 'sc-toggle-off'"
          :title="e.enabled ? 'Aktiv — klicken zum Deaktivieren' : 'Inaktiv — klicken zum Aktivieren'"
          @click="e.enabled = !e.enabled">
          {{ e.enabled ? 'AN' : 'AUS' }}
        </button>

        <!-- Weekday -->
        <div class="sc-wday-btns">
          <button
            v-for="(d, di) in WDAY_NAMES"
            :key="di"
            class="sc-wday-btn"
            :class="{ active: e.wday === di }"
            :title="WDAY_FULL[di]"
            @click="e.wday = di">
            {{ d }}
          </button>
        </div>

        <!-- Time range -->
        <div class="sc-time-row">
          <input
            class="sc-time-input"
            type="time"
            :value="hhmm2str(e.start_hhmm)"
            @change="onStartChange(e, ($event.target as HTMLInputElement).value)"
            title="Startzeit"
          />
          <span class="sc-time-sep">–</span>
          <input
            class="sc-time-input"
            type="time"
            :value="hhmm2str(e.end_hhmm)"
            @change="onEndChange(e, ($event.target as HTMLInputElement).value)"
            title="Endzeit"
          />
        </div>

        <!-- Rain delay -->
        <div class="sc-rain-row" title="Verzögerung nach Regen (Minuten)">
          <span class="sc-rain-lbl">Regen +</span>
          <input
            class="sc-num-input"
            type="number"
            min="0"
            max="240"
            step="10"
            :value="e.rain_delay_min"
            @change="e.rain_delay_min = parseInt(($event.target as HTMLInputElement).value) || 0"
          />
          <span class="sc-rain-unit">min</span>
        </div>

        <!-- Delete -->
        <button class="sc-del" title="Eintrag löschen" @click="removeEntry(i)">✕</button>
      </div>
    </div>

    <!-- Info box -->
    <div class="sc-info">
      Der Roboter startet automatisch zur konfigurierten Zeit aus dem Ladezustand
      und fährt bei Ende automatisch zur Ladestation. Regen-Verzögerung verschiebt den Start.
    </div>
  </div>
</template>

<style scoped>
.sc-wrap {
  padding: 16px; display: flex; flex-direction: column; gap: 14px;
  font-family: sans-serif; background: #0a0f1a !important; min-height: 100%;
}
.sc-header {
  display: flex; align-items: center; justify-content: space-between; gap: 10px;
}
.sc-title { font-size: 15px; font-weight: 600; color: #e2e8f0 !important; margin: 0; }
.sc-header-actions { display: flex; gap: 8px; }

/* Buttons */
.sc-btn {
  padding: 7px 14px; font-size: 12px; font-weight: 600; border-radius: 7px;
  border: 1px solid; cursor: pointer; font-family: sans-serif;
}
.sc-btn:disabled { opacity: 0.4; cursor: not-allowed; }
.sc-btn-add  { background: #0f2744 !important; border-color: #3b82f6; color: #93c5fd !important; }
.sc-btn-save { background: #14532d !important; border-color: #22c55e; color: #86efac !important; }

/* Toast */
.sc-toast {
  position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
  border-radius: 8px; padding: 10px 18px; font-size: 13px; z-index: 100; pointer-events: none;
  border: 1px solid;
}
.sc-toast-ok  { background: #14532d; border-color: #22c55e; color: #86efac !important; }
.sc-toast-err { background: #450a0a; border-color: #ef4444; color: #fca5a5 !important; }
.toast-enter-active, .toast-leave-active { transition: opacity 0.3s; }
.toast-enter-from, .toast-leave-to { opacity: 0; }

/* Empty state */
.sc-empty {
  text-align: center; padding: 40px 20px;
  color: #4b6a8a !important; font-size: 13px; display: flex;
  flex-direction: column; align-items: center; gap: 12px;
}
.sc-empty-icon { font-size: 40px; }

/* Entry list */
.sc-list { display: flex; flex-direction: column; gap: 10px; }
.sc-entry {
  background: #0f1829 !important; border: 1px solid #1e3a5f; border-radius: 10px;
  padding: 12px 14px; display: flex; align-items: center; gap: 10px; flex-wrap: wrap;
  transition: opacity 0.2s;
}
.sc-entry-disabled { opacity: 0.5; }

/* Toggle */
.sc-toggle {
  font-size: 10px; font-weight: 700; padding: 3px 8px; border-radius: 5px;
  border: 1px solid; cursor: pointer; font-family: sans-serif; min-width: 36px;
}
.sc-toggle-on  { background: #14532d !important; border-color: #22c55e; color: #86efac !important; }
.sc-toggle-off { background: #1e2d3d !important; border-color: #334155; color: #64748b !important; }

/* Weekday buttons */
.sc-wday-btns { display: flex; gap: 3px; }
.sc-wday-btn {
  width: 28px; height: 28px; font-size: 11px; font-weight: 600;
  border-radius: 5px; border: 1px solid #1e3a5f; background: #070d18 !important;
  color: #4b6a8a !important; cursor: pointer; font-family: sans-serif;
}
.sc-wday-btn.active {
  background: #1e3a5f !important; border-color: #3b82f6; color: #60a5fa !important;
}

/* Time inputs */
.sc-time-row { display: flex; align-items: center; gap: 5px; }
.sc-time-input {
  background: #070d18 !important; border: 1px solid #1e3a5f; border-radius: 6px;
  color: #e2e8f0 !important; font-size: 13px; padding: 4px 6px;
  font-family: monospace; width: 80px;
}
.sc-time-sep { color: #4b6a8a !important; font-size: 13px; }

/* Rain delay */
.sc-rain-row { display: flex; align-items: center; gap: 5px; margin-left: auto; }
.sc-rain-lbl  { font-size: 11px; color: #4b6a8a !important; }
.sc-rain-unit { font-size: 11px; color: #4b6a8a !important; }
.sc-num-input {
  background: #070d18 !important; border: 1px solid #1e3a5f; border-radius: 6px;
  color: #e2e8f0 !important; font-size: 12px; padding: 3px 5px; width: 52px;
  font-family: monospace;
}

/* Delete */
.sc-del {
  background: none; border: none; color: #4b6a8a !important; cursor: pointer;
  font-size: 14px; padding: 2px 4px; border-radius: 4px;
  margin-left: auto;
}
.sc-del:hover { color: #ef4444 !important; background: #1e0a0a !important; }

/* Info */
.sc-info {
  font-size: 12px; color: #4b6a8a !important; background: #0f1829 !important;
  border: 1px solid #1e2d3d; border-radius: 8px; padding: 10px 14px; line-height: 1.5;
}
</style>
