<script lang="ts">
  import { afterUpdate } from 'svelte'
  import { logStore } from '../../stores/logs'

  let open = false
  let logEl: HTMLDivElement | null = null
  let autoScroll = true

  function toggle() { open = !open }

  function onScroll() {
    if (!logEl) return
    autoScroll = logEl.scrollTop + logEl.clientHeight >= logEl.scrollHeight - 4
  }

  afterUpdate(() => {
    if (open && autoScroll && logEl) {
      logEl.scrollTop = logEl.scrollHeight
    }
  })

  function formatTime(ts: number): string {
    const d = new Date(ts)
    return d.toTimeString().slice(0, 8)
  }
</script>

<div class="logbar" class:open>
  <button type="button" class="logbar-toggle" on:click={toggle}>
    <span class="logbar-label">Log</span>
    <span class="logbar-count">{$logStore.length}</span>
    <span class="logbar-chevron">{open ? '▾' : '▴'}</span>
  </button>

  {#if open}
    <div class="logbar-body">
      <div
        class="logbar-entries"
        bind:this={logEl}
        on:scroll={onScroll}
        role="log"
        aria-live="polite"
        aria-label="Systemprotokoll"
      >
        {#if $logStore.length === 0}
          <span class="logbar-empty">Keine Einträge</span>
        {:else}
          {#each $logStore as entry (entry.id)}
            <div class="logbar-entry">
              <span class="logbar-ts">{formatTime(entry.ts)}</span>
              <span class="logbar-text">{entry.text}</span>
            </div>
          {/each}
        {/if}
      </div>
      <button
        type="button"
        class="logbar-clear"
        on:click={() => logStore.clear()}
        aria-label="Log leeren"
      >Leeren</button>
    </div>
  {/if}
</div>

<style>
  .logbar {
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    z-index: 10;
    display: flex;
    flex-direction: column-reverse;
  }

  .logbar-toggle {
    display: flex;
    align-items: center;
    gap: 7px;
    padding: 5px 14px;
    background: rgba(10, 16, 32, 0.92);
    border: none;
    border-top: 1px solid #1e3a5f;
    color: #64748b;
    font-size: 11px;
    cursor: pointer;
    text-align: left;
    backdrop-filter: blur(6px);
    transition: background 0.15s;
  }

  .logbar-toggle:hover { background: rgba(15, 24, 41, 0.96); color: #94a3b8; }
  .logbar.open .logbar-toggle { border-top: none; border-bottom: 1px solid #1e3a5f; color: #93c5fd; }

  .logbar-label {
    font-weight: 500;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 10px;
  }

  .logbar-count {
    background: #1e3a5f;
    color: #60a5fa;
    border-radius: 9px;
    padding: 1px 6px;
    font-size: 10px;
    font-family: monospace;
  }

  .logbar-chevron { margin-left: auto; font-size: 10px; }

  .logbar-body {
    display: flex;
    flex-direction: column;
    background: rgba(7, 13, 24, 0.97);
    border-top: 1px solid #1e3a5f;
    backdrop-filter: blur(6px);
  }

  .logbar-entries {
    height: 180px;
    overflow-y: auto;
    padding: 6px 0;
    display: flex;
    flex-direction: column;
  }

  .logbar-entry {
    display: flex;
    gap: 10px;
    padding: 2px 14px;
    font-size: 11px;
    font-family: monospace;
    line-height: 1.5;
  }

  .logbar-entry:hover { background: rgba(30, 58, 95, 0.2); }

  .logbar-ts {
    color: #475569;
    flex-shrink: 0;
  }

  .logbar-text { color: #94a3b8; }

  .logbar-empty {
    padding: 8px 14px;
    font-size: 11px;
    color: #334155;
    font-style: italic;
  }

  .logbar-clear {
    align-self: flex-end;
    margin: 4px 10px;
    padding: 3px 10px;
    background: transparent;
    border: 1px solid #1e3a5f;
    border-radius: 4px;
    color: #475569;
    font-size: 10px;
    cursor: pointer;
  }

  .logbar-clear:hover { border-color: #2563eb; color: #60a5fa; }
</style>
