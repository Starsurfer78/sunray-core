<script lang="ts">
  import { onMount } from 'svelte'
  import Dashboard from './lib/pages/Dashboard.svelte'
  import History from './lib/pages/History.svelte'
  import Map from './lib/pages/Map.svelte'
  import Mission from './lib/pages/Mission.svelte'
  import Settings from './lib/pages/Settings.svelte'
  import StatusBar from './lib/components/StatusBar.svelte'
  import GlobalInfoPanel from './lib/components/GlobalInfoPanel.svelte'
  import NotificationDisplay from './lib/components/NotificationDisplay.svelte'
  import LoadingStateDisplay from './lib/components/LoadingStateDisplay.svelte'
  import { sendCmd, startTelemetry, stopTelemetry } from './lib/api/websocket'
  import { telemetry } from './lib/stores/telemetry'
  import { connection } from './lib/stores/connection'
  import { joystickOpen } from './lib/stores/joystick'
  import { mapInfoOpen } from './lib/stores/mapInfo'
  import { toast } from './lib/stores/notificationStore'

  type View = 'dashboard' | 'history' | 'map' | 'mission' | 'settings'
  type NavItem = { id: View | null; label: string; enabled: boolean }

  let currentView: View = 'dashboard'
  let lastGlobalErrorKey = ''

  const navItems: NavItem[] = [
    { id: 'dashboard', label: 'Dashboard', enabled: true },
    { id: 'map', label: 'Karten', enabled: true },
    { id: 'mission', label: 'Missionen', enabled: true },
    { id: 'history', label: 'Verlauf', enabled: true },
    { id: null, label: 'Simulator', enabled: false },
  ]

  function triggerEmergencyStop() {
    if (!$connection.connected) {
      toast.warning('NOTAUS nicht gesendet: keine Live-Verbindung')
      return
    }

    const sent = sendCmd('stop')
    if (!sent) {
      toast.error('NOTAUS nicht gesendet: Verbindung nicht bereit')
      return
    }

    toast.success('✓ NOTAUS gesendet - Roboter stoppt sofort')
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

  $: {
    const errorKey = globalError
      ? `${globalError}|${globalErrorDetail}|${$connection.connected}`
      : ''

    if (globalError && $connection.connected && errorKey !== lastGlobalErrorKey) {
      lastGlobalErrorKey = errorKey
      toast.error(globalError, 8000, {
        label: 'Roboter zurücksetzen',
        handler: () => {
          sendCmd('reset')
          toast.info('Reset-Befehl gesendet...')
        },
      })
    } else if (!globalError) {
      lastGlobalErrorKey = ''
    }
  }

  function toggleJoystick() {
    joystickOpen.update((open) => !open)
  }

  function toggleMapInfo() {
    mapInfoOpen.update((open) => !open)
  }

  function openSettings() {
    currentView = 'settings'
  }

  onMount(() => {
    startTelemetry()
    return () => stopTelemetry()
  })
</script>

<main class="app-shell">
  <LoadingStateDisplay />
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

      <div class="topbar-div"></div>

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

      <button
        type="button"
        class="topbar-joystick"
        class:active={$joystickOpen}
        disabled={currentView !== 'dashboard'}
        title={
          currentView !== 'dashboard'
            ? 'Joystick ist im Dashboard verfuegbar'
            : !$connection.connected
              ? 'Joystick ohne Live-Verbindung nur als UI-Vorschau'
              : $joystickOpen
                ? 'Joystick ausblenden'
                : 'Joystick einblenden'
        }
        on:click={toggleJoystick}
      >
        <span aria-hidden="true">🕹</span>
      </button>

      <button
        type="button"
        class="topbar-tool-btn"
        class:active={currentView === 'settings'}
        title="Einstellungen und Diagnose"
        on:click={openSettings}
      >
        <span aria-hidden="true">⚙</span>
      </button>

      <button
        type="button"
        class="topbar-info-btn"
        class:active={$mapInfoOpen}
        title={
          $mapInfoOpen
            ? 'Info ausblenden'
            : 'Info einblenden'
        }
        on:click={toggleMapInfo}
      >
        <span aria-hidden="true">i</span>
      </button>
    </div>
  </header>

  <section class="view">
    <GlobalInfoPanel {currentView} />
    {#if currentView === 'dashboard'}
      <Dashboard />
    {:else if currentView === 'map'}
      <Map />
    {:else if currentView === 'mission'}
      <Mission />
    {:else if currentView === 'history'}
      <History />
    {:else if currentView === 'settings'}
      <Settings />
    {/if}
  </section>

  <nav class="bottom-nav" aria-label="Hauptnavigation Mobile">
    {#each navItems.filter(i => i.id !== null) as item}
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

  <NotificationDisplay />
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

  .topbar-div {
    width: 1px;
    height: 22px;
    background: #1e3a5f;
    flex-shrink: 0;
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

  .topbar-joystick {
    flex-shrink: 0;
    width: 2.4rem;
    height: 2.4rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #060c17;
    color: #93c5fd;
    font-size: 1rem;
    cursor: pointer;
  }

  .topbar-tool-btn {
    flex-shrink: 0;
    width: 2.4rem;
    height: 2.4rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #060c17;
    color: #94a3b8;
    font-size: 1rem;
    cursor: pointer;
  }

  .topbar-info-btn {
    flex-shrink: 0;
    width: 2.4rem;
    height: 2.4rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 0.55rem;
    border: 1px solid #1e3a5f;
    background: #060c17;
    color: #bfdbfe;
    font-size: 1rem;
    font-weight: 700;
    cursor: pointer;
  }

  .topbar-tool-btn.active {
    border-color: #60a5fa;
    background: #10213b;
    color: #dbeafe;
  }

  .topbar-info-btn.active {
    border-color: #0891b2;
    background: #082f49;
    color: #cffafe;
  }

  .topbar-info-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .topbar-tool-btn:hover {
    border-color: #60a5fa;
    color: #dbeafe;
  }

  .topbar-joystick.active {
    border-color: #22c55e;
    background: #0b3320;
    color: #dcfce7;
  }

  .topbar-joystick:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .view {
    min-width: 0;
    overflow: hidden;
  }

  @media (max-width: 900px) {
    .topbar-main {
      align-items: start;
      flex-direction: column;
    }

    .emergency {
      margin-left: 0;
    }
  }

  /* ── Mobile (≤640px) ── */
  .bottom-nav {
    display: none;
  }

  @media (max-width: 640px) {
    /* Topbar: hide tabs, divider, joystick */
    .topbar-div {
      display: none;
    }

    .tabs {
      display: none;
    }

    .topbar-joystick {
      display: none;
    }

    /* Reset column direction forced by 900px breakpoint */
    .topbar-main {
      flex-direction: row;
      align-items: center;
      flex-wrap: nowrap;
    }

    .emergency {
      margin-left: auto;
      min-height: 44px;
      align-self: stretch;
      display: flex;
      align-items: center;
    }

    /* Ensure tool buttons are tap-friendly */
    .topbar-tool-btn,
    .topbar-info-btn {
      width: 2.75rem;
      height: 2.75rem;
      min-height: 44px;
    }

    /* Give the view padding so content isn't hidden behind bottom nav */
    .view {
      padding-bottom: 56px;
    }

    /* Bottom nav bar */
    .bottom-nav {
      display: flex;
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      z-index: 30;
      background: #0a0f1a;
      border-top: 1px solid #1e3a5f;
      padding-bottom: env(safe-area-inset-bottom, 0px);
    }

    .bottom-nav button {
      flex: 1;
      border: 0;
      background: transparent;
      color: #7a8da8;
      font-size: 0.78rem;
      font-family: inherit;
      min-height: 44px;
      padding: 0.5rem 0.25rem;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .bottom-nav button.active {
      color: #93c5fd;
      background: #10213b;
      font-weight: 600;
    }

    .bottom-nav button.future {
      color: #3d5166;
      cursor: default;
    }

    .bottom-nav button:disabled {
      opacity: 0.5;
    }
  }
</style>
