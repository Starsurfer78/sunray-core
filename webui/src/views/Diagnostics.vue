<script setup lang="ts">
import { useTelemetry } from '../composables/useTelemetry'

const { telemetry } = useTelemetry()
</script>

<template>
  <div class="max-w-2xl space-y-4">
    <h2 class="text-lg font-semibold text-gray-200">Diagnose</h2>

    <!-- Live Telemetrie-Rohdaten -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-3">Telemetrie-Rohdaten</div>
      <pre class="text-xs font-mono text-green-400 overflow-auto max-h-64">{{ JSON.stringify(telemetry, null, 2) }}</pre>
    </div>

    <!-- Motor-Test Buttons (TODO: REST /api/cmd/motor-test) -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-3">Motor-Test</div>
      <p class="text-xs text-yellow-500 mb-3">
        ⚠ Nur bei freigegebenem Roboter verwenden!
      </p>
      <div class="flex flex-wrap gap-2">
        <button disabled
          class="px-4 py-2 bg-gray-800 text-gray-500 text-xs rounded cursor-not-allowed">
          Motor Links — folgt
        </button>
        <button disabled
          class="px-4 py-2 bg-gray-800 text-gray-500 text-xs rounded cursor-not-allowed">
          Motor Rechts — folgt
        </button>
        <button disabled
          class="px-4 py-2 bg-gray-800 text-gray-500 text-xs rounded cursor-not-allowed">
          Mähwerk — folgt
        </button>
      </div>
    </div>

    <!-- Log-Stream (TODO: GET /api/log SSE) -->
    <div class="bg-gray-900 border border-gray-800 rounded-lg p-4">
      <div class="text-xs text-gray-500 uppercase tracking-wider mb-2">Log-Stream</div>
      <p class="text-xs text-gray-500">
        Live-Log via Server-Sent Events — kommt nach REST-API Implementierung.
      </p>
    </div>
  </div>
</template>
