<script lang="ts">
  import { onMount } from 'svelte'
  import Diagnostics from './lib/pages/Diagnostics.svelte'
  import Dashboard from './lib/pages/Dashboard.svelte'
  import Map from './lib/pages/Map.svelte'
  import Mission from './lib/pages/Mission.svelte'
  import { startTelemetry, stopTelemetry } from './lib/api/websocket'

  type View = 'dashboard' | 'diagnostics' | 'map' | 'mission'
  let currentView: View = 'dashboard'

  const navItems: Array<{ id: View; label: string }> = [
    { id: 'dashboard', label: 'Dashboard' },
    { id: 'diagnostics', label: 'Diagnose' },
    { id: 'map', label: 'Karte' },
    { id: 'mission', label: 'Mission' },
  ]

  onMount(() => {
    startTelemetry()
    return () => stopTelemetry()
  })
</script>

<main class="shell">
  <aside class="sidebar">
    <div class="brand">
      <span class="eyebrow">Alfred</span>
      <strong>WebUI Svelte MVP</strong>
    </div>

    <nav>
      {#each navItems as item}
        <button
          type="button"
          class:active={currentView === item.id}
          on:click={() => currentView = item.id}
        >
          {item.label}
        </button>
      {/each}
    </nav>
  </aside>

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
  .shell {
    display: grid;
    grid-template-columns: 240px minmax(0, 1fr);
    min-height: 100vh;
  }

  .sidebar {
    display: grid;
    align-content: start;
    gap: 1.4rem;
    padding: 1.3rem;
    border-right: 1px solid rgba(155, 190, 172, 0.12);
    background: rgba(9, 16, 14, 0.76);
    backdrop-filter: blur(14px);
  }

  .brand {
    display: grid;
    gap: 0.35rem;
  }

  .eyebrow {
    color: #9fc892;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.8rem;
  }

  nav {
    display: grid;
    gap: 0.55rem;
  }

  nav button {
    text-align: left;
    padding: 0.85rem 0.95rem;
    border: 1px solid rgba(155, 190, 172, 0.12);
    border-radius: 0.9rem;
    background: rgba(17, 28, 25, 0.75);
    color: #dce8e8;
    cursor: pointer;
  }

  nav button.active {
    border-color: rgba(196, 218, 120, 0.28);
    background: rgba(75, 97, 47, 0.38);
    color: #e3efb8;
  }

  .view {
    min-width: 0;
  }

  @media (max-width: 900px) {
    .shell {
      grid-template-columns: 1fr;
    }

    .sidebar {
      border-right: 0;
      border-bottom: 1px solid rgba(155, 190, 172, 0.12);
    }

    nav {
      grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    }
  }
</style>
