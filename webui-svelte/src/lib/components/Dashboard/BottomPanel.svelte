<script lang="ts">
  import { afterUpdate } from "svelte";
  import { logStore } from "../../stores/logs";
  import { telemetry } from "../../stores/telemetry";
  import { commandFeedback } from "../../stores/commandFeedback";
  import { RAD_TO_DEG } from "../../utils/mapHelpers";

  let open = false;
  let activeTab: "log" | "gps" = "log";
  let logEl: HTMLDivElement | null = null;
  let autoScroll = true;
  const tabs: Array<{ id: "log" | "gps"; label: string }> = [
    { id: "log", label: "Log" },
    { id: "gps", label: "GPS" },
  ];

  function onScroll() {
    if (!logEl) return;
    autoScroll = logEl.scrollTop + logEl.clientHeight >= logEl.scrollHeight - 4;
  }

  afterUpdate(() => {
    if (activeTab === "log" && open && autoScroll && logEl) {
      logEl.scrollTop = logEl.scrollHeight;
    }
  });

  function formatTime(ts: number): string {
    return new Date(ts).toTimeString().slice(0, 8);
  }

  $: gpsFixLabel =
    $telemetry.gps_sol === 4
      ? "RTK-Fix"
      : $telemetry.gps_sol === 5
        ? "RTK-Float"
        : "Kein Fix";
  $: gpsFixColor =
    $telemetry.gps_sol === 4
      ? "#4ade80"
      : $telemetry.gps_sol === 5
        ? "#fbbf24"
        : "#f87171";
  $: headingDeg =
    (((($telemetry.heading ?? 0) * RAD_TO_DEG) % 360) + 360) % 360;
</script>

<div class="bp" class:open>
  {#if $commandFeedback}
    <div
      class={`bp-notice ${$commandFeedback.tone}`}
      role="status"
      aria-live="polite"
    >
      {$commandFeedback.message}
    </div>
  {/if}

  <!-- Tab-Bar / Toggle -->
  <div class="bp-bar">
    <div class="bp-tabs">
      {#each tabs as { id, label }}
        <button
          type="button"
          class="bp-tab"
          class:active={activeTab === id}
          on:click={() => {
            activeTab = id;
            open = true;
          }}
        >
          {label}
          {#if id === "log" && $logStore.length > 0}
            <span class="bp-badge">{$logStore.length}</span>
          {/if}
        </button>
      {/each}
    </div>
    <button type="button" class="bp-toggle" on:click={() => (open = !open)}>
      {open ? "▾" : "▴"}
    </button>
  </div>

  {#if open}
    <div class="bp-body">
      <!-- Log -->
      {#if activeTab === "log"}
        <div
          class="bp-log"
          bind:this={logEl}
          on:scroll={onScroll}
          role="log"
          aria-live="polite"
        >
          {#if $logStore.length === 0}
            <span class="bp-empty">Keine Einträge</span>
          {:else}
            {#each $logStore as entry (entry.id)}
              <div class="bp-log-entry">
                <span class="bp-ts">{formatTime(entry.ts)}</span>
                <span class="bp-log-text">{entry.text}</span>
              </div>
            {/each}
          {/if}
        </div>
        <div class="bp-foot">
          <button
            type="button"
            class="bp-clear"
            on:click={() => logStore.clear()}>Leeren</button
          >
        </div>

        <!-- GPS Detail -->
      {:else if activeTab === "gps"}
        <div class="bp-gps">
          <div class="bp-grow">
            <div class="bp-gcell">
              <div class="bp-glbl">Fix</div>
              <div class="bp-gval" style="color: {gpsFixColor}">
                {gpsFixLabel}
              </div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">Solution</div>
              <div class="bp-gval">{$telemetry.gps_sol}</div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">Status</div>
              <div class="bp-gval bp-gsmall">{$telemetry.gps_text || "—"}</div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">Breitengrad</div>
              <div class="bp-gval bp-gmono">
                {$telemetry.gps_lat.toFixed(8)}°
              </div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">Längengrad</div>
              <div class="bp-gval bp-gmono">
                {$telemetry.gps_lon.toFixed(8)}°
              </div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">X lokal</div>
              <div class="bp-gval bp-gmono">{$telemetry.x.toFixed(3)} m</div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">Y lokal</div>
              <div class="bp-gval bp-gmono">{$telemetry.y.toFixed(3)} m</div>
            </div>
            <div class="bp-gcell">
              <div class="bp-glbl">Richtung</div>
              <div class="bp-gval bp-gmono">{headingDeg.toFixed(1)}°</div>
            </div>
          </div>
        </div>
      {/if}
    </div>
  {/if}
</div>

<style>
  .bp {
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
    border-top: 1px solid #1e3a5f;
  }

  .bp-notice {
    padding: 0.55rem 0.8rem;
    font-size: 0.78rem;
    line-height: 1.4;
    border-bottom: 1px solid #1e3a5f;
  }

  .bp-notice.error {
    background: rgba(69, 10, 10, 0.92);
    color: #fecaca;
  }

  .bp-bar {
    display: flex;
    align-items: center;
    background: rgba(10, 16, 32, 0.97);
    backdrop-filter: blur(6px);
    flex-shrink: 0;
  }

  .bp-tabs {
    display: flex;
    flex: 1;
    overflow: hidden;
  }

  .bp-tab {
    display: flex;
    align-items: center;
    gap: 5px;
    padding: 5px 13px;
    background: transparent;
    border: none;
    border-right: 1px solid #1a2a40;
    color: #64748b;
    font-size: 10px;
    font-weight: 500;
    text-transform: uppercase;
    letter-spacing: 0.07em;
    cursor: pointer;
    white-space: nowrap;
    transition:
      color 0.15s,
      background 0.15s;
  }

  .bp-tab:hover {
    color: #94a3b8;
    background: rgba(30, 58, 95, 0.3);
  }
  .bp-tab.active {
    color: #60a5fa;
    background: rgba(15, 24, 41, 0.8);
    border-bottom: 2px solid #2563eb;
  }

  .bp-badge {
    background: #1e3a5f;
    color: #60a5fa;
    border-radius: 8px;
    padding: 1px 5px;
    font-size: 9px;
    font-family: monospace;
  }

  .bp-toggle {
    padding: 5px 12px;
    background: transparent;
    border: none;
    border-left: 1px solid #1a2a40;
    color: #475569;
    font-size: 10px;
    cursor: pointer;
    flex-shrink: 0;
  }
  .bp-toggle:hover {
    color: #94a3b8;
  }

  .bp-body {
    background: rgba(7, 13, 24, 0.97);
    backdrop-filter: blur(6px);
    display: flex;
    flex-direction: column;
    height: 160px;
  }

  /* Log */
  .bp-log {
    flex: 1;
    overflow-y: auto;
    padding: 5px 0;
    display: flex;
    flex-direction: column;
  }

  .bp-log-entry {
    display: flex;
    gap: 10px;
    padding: 2px 14px;
    font-size: 11px;
    font-family: monospace;
    line-height: 1.5;
  }
  .bp-log-entry:hover {
    background: rgba(30, 58, 95, 0.2);
  }

  .bp-ts {
    color: #475569;
    flex-shrink: 0;
  }
  .bp-log-text {
    color: #94a3b8;
  }

  .bp-foot {
    display: flex;
    justify-content: flex-end;
    padding: 3px 10px 4px;
    border-top: 1px solid #0f1829;
    flex-shrink: 0;
  }

  .bp-clear {
    background: transparent;
    border: 1px solid #1e3a5f;
    border-radius: 4px;
    color: #475569;
    font-size: 10px;
    padding: 2px 8px;
    cursor: pointer;
  }
  .bp-clear:hover {
    border-color: #2563eb;
    color: #60a5fa;
  }

  /* GPS */
  .bp-gps {
    flex: 1;
    overflow-y: auto;
    padding: 8px;
  }
  .bp-grow {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 6px;
  }
  .bp-gcell {
    background: #0f1829;
    border: 1px solid #1a2a40;
    border-radius: 6px;
    padding: 6px 8px;
  }
  .bp-glbl {
    font-size: 9px;
    color: #475569;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    margin-bottom: 3px;
  }
  .bp-gval {
    font-size: 13px;
    font-weight: 500;
    color: #93c5fd;
  }
  .bp-gsmall {
    font-size: 11px;
  }
  .bp-gmono {
    font-family: monospace;
  }

  .bp-empty {
    padding: 10px 14px;
    font-size: 11px;
    color: #334155;
    font-style: italic;
  }
</style>
