<script lang="ts">
  import { onMount } from 'svelte'
  import { getConfigDocument, updateConfigDocument } from '../../api/rest'

  let dockAutoStart = true
  let loading = true
  let saving = false
  let info = ''
  let error = ''

  async function loadDockSettings() {
    loading = true
    error = ''
    try {
      const config = await getConfigDocument()
      dockAutoStart = typeof config.dock_auto_start === 'boolean' ? config.dock_auto_start : true
    } catch (loadError) {
      error = loadError instanceof Error ? loadError.message : 'Dock-Einstellung konnte nicht geladen werden'
    } finally {
      loading = false
    }
  }

  async function onToggleDockAutoStart(event: Event) {
    const nextValue = (event.currentTarget as HTMLInputElement).checked
    const previousValue = dockAutoStart
    dockAutoStart = nextValue
    saving = true
    error = ''
    info = ''

    try {
      const result = await updateConfigDocument({ dock_auto_start: nextValue })
      if (!result.ok) {
        throw new Error(result.error ?? 'Dock-Einstellung konnte nicht gespeichert werden')
      }
      info = nextValue
        ? 'Nach dem Laden wird wieder automatisch gemaeht.'
        : 'Nach dem Laden bleibt der Roboter an der Station.'
    } catch (saveError) {
      dockAutoStart = previousValue
      error = saveError instanceof Error ? saveError.message : 'Dock-Einstellung konnte nicht gespeichert werden'
    } finally {
      saving = false
    }
  }

  onMount(() => {
    void loadDockSettings()
  })
</script>

<section class="dock-panel">
  <div class="dock-title">Docking</div>

  <label class="dock-toggle">
    <input
      type="checkbox"
      checked={dockAutoStart}
      disabled={loading || saving}
      on:change={onToggleDockAutoStart}
    />
    <div class="dock-copy">
      <strong>Nach Laden automatisch weiter maehen</strong>
      <span>Wenn deaktiviert, bleibt der Roboter nach erfolgreichem Laden an der Station.</span>
    </div>
  </label>

  {#if loading}
    <div class="dock-note">Lade Einstellung ...</div>
  {:else if saving}
    <div class="dock-note">Speichere Einstellung ...</div>
  {:else if error}
    <div class="dock-note error">{error}</div>
  {:else if info}
    <div class="dock-note ok">{info}</div>
  {/if}
</section>

<style>
  .dock-panel {
    display: grid;
    gap: 0.45rem;
    padding: 0.58rem 0.68rem;
    border-bottom: 1px solid #0f1829;
  }

  .dock-title {
    color: #7a8da8;
    font-size: 0.59rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .dock-toggle {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.65rem;
    align-items: start;
    padding: 0.7rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.6rem;
    background: #0a1020;
    color: #dbeafe;
  }

  .dock-toggle input {
    margin-top: 0.15rem;
  }

  .dock-copy {
    display: grid;
    gap: 0.2rem;
  }

  .dock-copy strong {
    font-size: 0.8rem;
    color: #dbeafe;
  }

  .dock-copy span {
    font-size: 0.72rem;
    color: #94a3b8;
    line-height: 1.4;
  }

  .dock-note {
    font-size: 0.72rem;
    color: #94a3b8;
  }

  .dock-note.ok {
    color: #4ade80;
  }

  .dock-note.error {
    color: #fca5a5;
  }
</style>
