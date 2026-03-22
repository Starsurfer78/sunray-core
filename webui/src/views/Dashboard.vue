<script setup lang="ts">
import { ref, computed } from 'vue'
import { useTelemetry } from '../composables/useTelemetry'
import RobotMap from '../components/RobotMap.vue'

const { telemetry } = useTelemetry()
const mapRef = ref<InstanceType<typeof RobotMap> | null>(null)

const mapBadge = computed(() =>
  telemetry.value.gps_lat !== 0
    ? `${telemetry.value.gps_lat.toFixed(4)}°N  ${telemetry.value.gps_lon.toFixed(4)}°E`
    : 'Kein GPS'
)
</script>

<template>
  <div class="sr-dash">
    <RobotMap
      ref="mapRef"
      :x="telemetry.x"
      :y="telemetry.y"
      :heading="telemetry.heading"
      :op="telemetry.op"
    />
    <!-- GPS badge -->
    <div class="sr-mbadge">{{ mapBadge }}</div>
    <!-- Zoom controls -->
    <div class="sr-mctrl">
      <button class="sr-mcb" @click="mapRef?.zoomIn()">+</button>
      <button class="sr-mcb" @click="mapRef?.zoomOut()">−</button>
      <button class="sr-mcb" title="Roboter zentrieren" @click="mapRef?.centerRobot()">⊙</button>
    </div>
  </div>
</template>

<style scoped>
.sr-dash {
  flex: 1;
  position: relative;
  background: #070d18 !important;
  overflow: hidden;
  height: 100%;
}

.sr-mbadge {
  position: absolute; top: 10px; left: 10px;
  background: rgba(15,24,41,0.88); border: 1px solid #1e3a5f;
  border-radius: 7px; padding: 6px 10px; font-size: 11px;
  color: #60a5fa !important; pointer-events: none; font-family: sans-serif;
}

.sr-mctrl {
  position: absolute; bottom: 12px; left: 12px; display: flex; gap: 5px;
}
.sr-mcb {
  background: #0f1829 !important; border: 1px solid #1e3a5f;
  color: #60a5fa !important; border-radius: 6px; padding: 5px 9px;
  font-size: 13px; cursor: pointer; font-family: sans-serif;
}
</style>
