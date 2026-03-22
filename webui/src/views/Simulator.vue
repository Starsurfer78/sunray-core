<script setup lang="ts">
import { ref } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'

const { telemetry } = useTelemetry()

// Sim-Steuerung über REST /api/sim/* (TODO: REST-Endpoints in Crow)
const gpsQuality = ref<'fix' | 'float' | 'none'>('fix')
const simError   = ref('')

async function simCmd(action: string, body?: object) {
  simError.value = ''
  try {
    const res = await fetch(`/api/sim/${action}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    body ? JSON.stringify(body) : undefined,
    })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
  } catch (e) {
    simError.value = String(e)
  }
}
</script>

<template>
  <div class="max-w-xl space-y-4">
    <div class="flex items-center gap-3">
      <h2 class="text-lg font-semibold text-gray-200">Simulator</h2>
      <span class="px-2 py-0.5 text-xs bg-purple-900 text-purple-300 rounded">
        Nur im --sim Modus aktiv
      </span>
    </div>

    <div v-if="simError"
      class="bg-red-900/30 border border-red-800 text-red-300 text-xs p-3 rounded">
      {{ simError }}
    </div>

    <!-- Bumper-Auslösung -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-3">Bumper auslösen</div>
      <div class="flex gap-3">
        <button @click="simCmd('bumper', { side: 'left' })"
          class="px-5 py-2 bg-orange-800 hover:bg-orange-700 text-white text-sm rounded">
          ◄ Bumper Links
        </button>
        <button @click="simCmd('bumper', { side: 'right' })"
          class="px-5 py-2 bg-orange-800 hover:bg-orange-700 text-white text-sm rounded">
          Bumper Rechts ►
        </button>
        <button @click="simCmd('bumper', { side: 'both' })"
          class="px-5 py-2 bg-orange-900 hover:bg-orange-800 text-white text-sm rounded">
          ◄► Beide
        </button>
      </div>
    </div>

    <!-- GPS-Qualität -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-3">GPS-Qualität</div>
      <div class="flex gap-3">
        <button
          v-for="q in ['fix', 'float', 'none'] as const"
          :key="q"
          @click="gpsQuality = q; simCmd('gps', { quality: q })"
          :class="['px-4 py-2 text-sm rounded transition-colors',
            gpsQuality === q
              ? 'bg-green-700 text-white'
              : 'bg-gray-800 text-gray-400 hover:bg-gray-700']"
        >
          {{ q === 'fix' ? 'RTK-Fix' : q === 'float' ? 'RTK-Float' : 'Kein GPS' }}
        </button>
      </div>
      <div class="mt-2 text-xs text-gray-500">
        Aktuell: <span class="text-gray-300">{{ telemetry.gps_text }}</span>
        (sol={{ telemetry.gps_sol }})
      </div>
    </div>

    <!-- Lift-Sensor -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-3">Lift-Sensor</div>
      <button @click="simCmd('lift')"
        class="px-5 py-2 bg-yellow-800 hover:bg-yellow-700 text-white text-sm rounded">
        ⬆ Lift auslösen
      </button>
    </div>

    <!-- Aktuelle Position -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-2">Aktuelle Sim-Position</div>
      <div class="font-mono text-sm text-gray-300">
        x={{ telemetry.x.toFixed(3) }}m &nbsp;
        y={{ telemetry.y.toFixed(3) }}m &nbsp;
        hdg={{ Math.round(telemetry.heading * 180 / Math.PI) }}°
      </div>
      <div class="text-xs text-gray-500 mt-1">Op: {{ telemetry.op }}</div>
    </div>

  </div>
</template>
