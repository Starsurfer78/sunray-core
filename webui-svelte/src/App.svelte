<script lang="ts">
  import { onMount } from 'svelte'
  import Diagnostics from './lib/pages/Diagnostics.svelte'
  import Dashboard from './lib/pages/Dashboard.svelte'
  import Map from './lib/pages/Map.svelte'
  import Mission from './lib/pages/Mission.svelte'
  import StatusBar from './lib/components/StatusBar.svelte'
  import { startTelemetry, stopTelemetry } from './lib/api/websocket'
  import { telemetry } from './lib/stores/telemetry'

  type View = 'dashboard' | 'diagnostics' | 'map' | 'mission'
  type NavItem = { id: View | null; label: string; enabled: boolean }

  let currentView: View = 'dashboard'
  let emergencyInfo = ''

  const navItems: NavItem[] = [
    { id: 'dashboard', label: 'Dashboard', enabled: true },
    { id: 'map', label: 'Karten', enabled: true },
    { id: null, label: 'Verlauf', enabled: false },
    { id: 'diagnostics', label: 'Diagnose', enabled: true },
    { id: null, label: 'Simulator', enabled: false },
    { id: 'mission', label: 'Mission', enabled: true },
    { id: null, label: 'Einstellungen', enabled: false },
  ]

  function triggerEmergencyStop() {
    emergencyInfo = 'NOTAUS ist aktuell noch ein Platzhalter'
    window.setTimeout(() => {
      if (emergencyInfo === 'NOTAUS ist aktuell noch ein Platzhalter') emergencyInfo = ''
    }, 2600)
  }

  function humanizeReason(reason: string) {
    return reason
      .replace(/^ERR_/, '')
      .replace(/_/g, ' ')
      .toLowerCase()
      .replace(/^\w/, (c) => c.toUpperCase())
  }

  $: globalError =
    $telemetry.op === 'Error'
      ? ($telemetry.error_code || humanizeReason($telemetry.event_reason || 'Unbekannter Fehler'))
      : ''

  $: globalErrorDetail =
    $telemetry.op === 'Error' && $telemetry.event_reason
      ? humanizeReason($telemetry.event_reason)
      : ''

  onMount(() => {
    startTelemetry()
    return () => stopTelemetry()
  })
</script>

<main class="app-shell">
  <header class="topbar">
    <div class="topbar-main">
      <div class="brand">
        <span class="brand-mark">Alfred</span>
        <span class="brand-sub">Sunray</span>
      </div>

      <StatusBar compact />

      <button type="button" class="emergency" on:click={triggerEmergencyStop}>
        NOTAUS
      </button>
    </div>

    <div class="topbar-nav">
      <nav class="tabs" aria-label="Hauptnavigation">
        {#each navItems as item}
          <button
            type="button"
            class:active={item.id !== null && currentView === item.id}
            class:future={!item.enabled}
            disabled={!item.enabled}
            on:click={() => {
              if (item.id) currentView = item.id
            }}
          >
            {item.label}
          </button>
        {/each}
      </nav>

      {#if emergencyInfo}
        <span class="topbar-info">{emergencyInfo}</span>
      {/if}
    </div>

    {#if globalError}
      <div class="error-banner" role="alert" aria-live="assertive">
        <strong>Fehler:</strong>
        <span>{globalError}</span>
        {#if globalErrorDetail && globalErrorDetail !== globalError}
          <small>{globalErrorDetail}</small>
        {/if}
      </div>
    {/if}
  </header>

  <section class="view">
    {#if currentView === 'dashboard'}
      <Dashboard />
    {:else if currentView === 'diagnostics'}
      <Diagnostics />
    {:else if currentView === 'map'}
      <Map />
    {:else if currentView === 'mission'}
      <Mission />
    {/if}
  </section>
</main>

<style>
  :global(body) {
    margin: 0;
    background: #0a0f1a;
    color: #e2e8f0;
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  }

  .app-shell {
    height: 100dvh;
    display: grid;
    grid-template-rows: auto 1fr;
    overflow: hidden;
    background:
      radial-gradient(circle at top, rgba(30, 58, 95, 0.38), transparent 34%),
      #0a0f1a;
  }

  .topbar {
    position: sticky;
    top: 0;
    z-index: 20;
    display: grid;
    gap: 0.6rem;
    padding: 0.85rem 1rem 0.7rem;
    background: rgba(10, 15, 26, 0.96);
    border-bottom: 1px solid #1e3a5f;
    backdrop-filter: blur(10px);
  }

  .topbar-main {
    display: flex;
    align-items: center;
    gap: 0.9rem;
    min-width: 0;
  }

  .brand {
    display: grid;
    gap: 0.05rem;
    flex-shrink: 0;
  }

  .brand-mark {
    color: #60a5fa;
    font-size: 0.98rem;
    font-weight: 700;
    letter-spacing: 0.03em;
  }

  .brand-sub {
    color: #7a8da8;
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 0.14em;
  }

  .emergency {
    margin-left: auto;
    flex-shrink: 0;
    padding: 0.5rem 0.95rem;
    border-radius: 0.5rem;
    border: 1.5px solid #dc2626;
    background: #450a0a;
    color: #fca5a5;
    font-weight: 700;
    cursor: pointer;
  }

  .topbar-nav {
    display: flex;
    align-items: center;
    gap: 0.8rem;
    min-width: 0;
  }

  .tabs {
    display: flex;
    flex-wrap: wrap;
    gap: 0.15rem;
    padding: 0.15rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.5rem;
    background: #060c17;
  }

  .tabs button {
    border: 0;
    border-radius: 0.38rem;
    padding: 0.45rem 0.78rem;
    background: transparent;
    color: #d7e4f2;
    font-size: 0.84rem;
    cursor: pointer;
  }

  .tabs button.active {
    background: #1e3a5f;
    color: #93c5fd;
    font-weight: 600;
  }

  .tabs button.future {
    color: #607188;
    cursor: default;
  }

  .tabs button:disabled {
    opacity: 0.65;
  }

  .topbar-info {
    color: #94a3b8;
    font-size: 0.78rem;
    white-space: nowrap;
  }

  .view {
    min-width: 0;
    overflow: hidden;
  }

  .error-banner {
    display: flex;
    align-items: center;
    gap: 0.65rem;
    flex-wrap: wrap;
    padding: 0.6rem 0.8rem;
    border: 1px solid #dc2626;
    border-radius: 0.55rem;
    background: rgba(69, 10, 10, 0.88);
    color: #fecaca;
    font-size: 0.84rem;
  }

  .error-banner strong {
    color: #fca5a5;
  }

  .error-banner small {
    color: #fda4af;
  }

  @media (max-width: 900px) {
    .topbar-main,
    .topbar-nav {
      align-items: start;
      flex-direction: column;
    }

    .emergency {
      margin-left: 0;
    }
  }
</style>
