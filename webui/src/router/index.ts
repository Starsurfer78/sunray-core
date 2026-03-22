import { createRouter, createWebHashHistory } from 'vue-router'
import Dashboard   from '../views/Dashboard.vue'
import MapEditor   from '../views/MapEditor.vue'
import Scheduler   from '../views/Scheduler.vue'
import History     from '../views/History.vue'
import Statistics  from '../views/Statistics.vue'
import Diagnostics from '../views/Diagnostics.vue'
import Simulator   from '../views/Simulator.vue'
import Settings    from '../views/Settings.vue'

export const routes = [
  { path: '/',           component: Dashboard,   name: 'dashboard',   label: 'Dashboard',    icon: '⚡' },
  { path: '/map',        component: MapEditor,   name: 'map',         label: 'Karte',         icon: '🗺' },
  { path: '/schedule',   component: Scheduler,   name: 'schedule',    label: 'Zeitpläne',     icon: '🕐' },
  { path: '/history',    component: History,     name: 'history',     label: 'Verlauf',       icon: '📋' },
  { path: '/statistics', component: Statistics,  name: 'statistics',  label: 'Statistiken',   icon: '📊' },
  { path: '/diagnostics',component: Diagnostics, name: 'diagnostics', label: 'Diagnose',      icon: '🔧' },
  { path: '/simulator',  component: Simulator,   name: 'simulator',   label: 'Simulator',     icon: '🎮' },
  { path: '/settings',   component: Settings,    name: 'settings',    label: 'Einstellungen', icon: '⚙' },
]

export default createRouter({
  history: createWebHashHistory(),
  routes,
})
