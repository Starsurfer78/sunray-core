<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRoute } from 'vue-router'
import { routes } from './router/index'
import { useTelemetry } from './composables/useTelemetry'
import RobotSidebar from './components/RobotSidebar.vue'
import LogPanel from './components/LogPanel.vue'

const route = useRoute()
const { telemetry, connected, sendCmd } = useTelemetry()

const battPct = computed(() => {
  const pct = ((telemetry.value.battery_v - 22) / (29.4 - 22)) * 100
  return Math.max(0, Math.min(100, Math.round(pct)))
})

const opLabel = computed(() => {
  const labels: Record<string, string> = {
    Mow: 'Mähend', Dock: 'Dockend', Charge: 'Laden',
    Error: 'Fehler', EscapeReverse: 'Ausweichen',
    GpsWait: 'GPS-Warten', Idle: 'Bereit',
  }
  return labels[telemetry.value.op] ?? telemetry.value.op
})

const apiToken = ref(localStorage.getItem('sunray_api_token') ?? '')

function saveApiToken() {
  const value = apiToken.value.trim()
  if (value) localStorage.setItem('sunray_api_token', value)
  else localStorage.removeItem('sunray_api_token')
  window.location.reload()
}
</script>

<template>
  <div class="sr-app">
    <header class="sr-top">
      <span class="sr-logo">Sunray</span>

      <span class="sr-pill" :class="connected ? 'sr-pill-conn' : 'sr-pill-err'">
        <span class="sr-dot"></span>{{ connected ? 'Verbunden' : 'Getrennt' }}
      </span>
      <span class="sr-pill sr-pill-op">{{ opLabel }}</span>
      <span class="sr-pill sr-pill-bat">
        {{ telemetry.battery_v.toFixed(1) }} V · {{ battPct }}%
      </span>

      <button class="sr-notaus" @click="sendCmd('stop')">NOTAUS</button>
      <label class="sr-token">
        <span class="sr-token-label">API-Token</span>
        <input
          v-model="apiToken"
          class="sr-token-input"
          type="password"
          placeholder="z. B. change-me-before-release"
          @keyup.enter="saveApiToken"
        >
        <button class="sr-token-btn" @click="saveApiToken">Speichern</button>
      </label>
      <span class="sr-div"></span>

      <nav class="sr-nav">
        <router-link
          v-for="r in routes"
          :key="r.path"
          :to="r.path"
          class="sr-nb"
          :class="{ on: route.path === r.path }"
        >{{ r.label }}</router-link>
      </nav>
    </header>

    <div class="sr-body">
      <div class="sr-left">
        <main class="sr-content">
          <router-view />
        </main>
        <LogPanel />
      </div>
      <RobotSidebar />
    </div>
  </div>
</template>

<style scoped>
*, *::before, *::after { box-sizing: border-box; }

.sr-app {
  display: flex;
  flex-direction: column;
  height: 100vh;
  background: #0a0f1a !important;
  color: #e2e8f0 !important;
  overflow: hidden;
  font-family: sans-serif;
}

/* ── Topbar ── */
.sr-top {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 9px 16px;
  background: #0f1829 !important;
  border-bottom: 1px solid #1e3a5f;
  flex-shrink: 0;
}

.sr-logo {
  font-size: 15px;
  font-weight: 500;
  color: #60a5fa !important;
  flex-shrink: 0;
}

.sr-pill {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  font-size: 11px;
  padding: 3px 9px;
  border-radius: 20px;
  font-weight: 500;
  flex-shrink: 0;
}
.sr-pill-conn { background: #052e16 !important; color: #4ade80 !important; border: 1px solid #166534; }
.sr-pill-err  { background: #1a0505 !important; color: #f87171 !important; border: 1px solid #450a0a; }
.sr-pill-op   { background: #0c1a3a !important; color: #93c5fd !important; border: 1px solid #1e40af; }
.sr-pill-bat  { background: #1e1b30 !important; color: #c4b5fd !important; border: 1px solid #312e81; }

.sr-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: currentColor;
  flex-shrink: 0;
}

.sr-notaus {
  background: #450a0a !important;
  border: 1.5px solid #dc2626 !important;
  color: #fca5a5 !important;
  border-radius: 7px;
  padding: 6px 14px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  flex-shrink: 0;
  margin-left: auto;
}

.sr-token {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  color: #94a3b8 !important;
  font-size: 11px;
  flex-shrink: 0;
}

.sr-token-label {
  white-space: nowrap;
}

.sr-token-input {
  width: 190px;
  padding: 5px 8px;
  border-radius: 6px;
  border: 1px solid #1e3a5f;
  background: #08111f !important;
  color: #e2e8f0 !important;
}

.sr-token-input::placeholder {
  color: #475569 !important;
}

.sr-token-btn {
  padding: 5px 9px;
  border-radius: 6px;
  border: 1px solid #1e3a5f;
  background: #0f1829 !important;
  color: #93c5fd !important;
  cursor: pointer;
}

.sr-token-btn:hover {
  border-color: #3b82f6;
}

.sr-div {
  width: 1px;
  height: 22px;
  background: #1e3a5f;
  flex-shrink: 0;
}

.sr-nav {
  display: flex;
  gap: 1px;
  background: #060c17 !important;
  border-radius: 7px;
  padding: 2px;
  border: 1px solid #1e3a5f;
  flex-shrink: 0;
}

.sr-nb {
  font-size: 12px;
  padding: 5px 11px;
  border-radius: 5px;
  cursor: pointer;
  background: transparent !important;
  color: #e2e8f0 !important;
  white-space: nowrap;
  text-decoration: none;
  border: none;
  display: inline-block;
}
.sr-nb.on {
  background: #1e3a5f !important;
  color: #93c5fd !important;
  font-weight: 500;
}

/* ── Body (below topbar) ── */
.sr-body {
  flex: 1;
  min-height: 0;
  display: flex;
  overflow: hidden;
}

/* ── Left column (content + log panel) ── */
.sr-left {
  flex: 1;
  min-height: 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* ── Content area ── */
.sr-content {
  flex: 1;
  min-height: 0;
  overflow: auto;
  display: flex;
  flex-direction: column;
}

@media (max-width: 1280px) {
  .sr-top {
    flex-wrap: wrap;
  }

  .sr-token {
    order: 2;
    width: 100%;
  }
}
</style>
