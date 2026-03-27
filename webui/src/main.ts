import { createApp } from 'vue'
import './style.css'
import App from './App.vue'
import router from './router'

const originalFetch = window.fetch.bind(window)
window.fetch = (input: RequestInfo | URL, init?: RequestInit) => {
  const token = localStorage.getItem('sunray_api_token') ?? ''
  if (!token) return originalFetch(input, init)

  const url = typeof input === 'string'
    ? input
    : (input instanceof URL ? input.toString() : input.url)
  const isApi = url.startsWith('/api/') || url.includes('/api/')
  if (!isApi) return originalFetch(input, init)

  const headers = new Headers(init?.headers ?? (input instanceof Request ? input.headers : undefined))
  if (!headers.has('Authorization')) {
    headers.set('Authorization', `Bearer ${token}`)
  }
  return originalFetch(input, { ...init, headers })
}

createApp(App).use(router).mount('#app')
