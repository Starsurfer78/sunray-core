<script lang="ts">
  import { sendCmd } from "../api/websocket";
  import { connection } from "../stores/connection";
  import { joystickOpen } from "../stores/joystick";

  export let startAllowed = true;
  export let startHint = "";
  /** missionId der aktuell ausgewählten Mission. Wenn gesetzt, wird start mit missionId gesendet. */
  export let missionId = "";

  const controls = [
    { label: "Start", cmd: "start", accent: "start" },
    { label: "Stop", cmd: "stop", accent: "stop" },
    { label: "Dock", cmd: "dock", accent: "dock" },
  ] as const;

  let speed = 0.5; // 0.1 – 1.0

  // Drag state for the panel
  let panelX = 0;
  let panelY = 0;
  let positionInitialized = false;

  function placeJoystickPanel() {
    const width = 210;
    const height = 260;
    const padding = 16;
    const topbarOffset = 88;
    panelX = Math.max(
      padding,
      Math.round(window.innerWidth / 2 - width / 2),
    );
    panelY = Math.max(
      topbarOffset,
      Math.round(window.innerHeight / 2 - height / 2),
    );
    positionInitialized = true;
  }

  function openJoystick() {
    placeJoystickPanel();
    joystickOpen.set(true);
  }
  let dragging = false;
  let dragOffsetX = 0;
  let dragOffsetY = 0;

  function onPanelPointerDown(e: PointerEvent) {
    dragging = true;
    dragOffsetX = e.clientX - panelX;
    dragOffsetY = e.clientY - panelY;
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  }

  function onPanelPointerMove(e: PointerEvent) {
    if (!dragging) return;
    panelX = e.clientX - dragOffsetX;
    panelY = e.clientY - dragOffsetY;
  }

  function onPanelPointerUp() {
    dragging = false;
  }

  // Joystick state
  let joystickEl: HTMLDivElement;
  let active = false;
  let knobX = 0;
  let knobY = 0;
  const radius = 44;
  const deadzone = 0.08;
  const sendInterval = 100;
  let intervalId: ReturnType<typeof setInterval> | null = null;

  function clamp(v: number, lo: number, hi: number) {
    return Math.max(lo, Math.min(hi, v));
  }

  function startSending() {
    if (intervalId) return;
    intervalId = setInterval(() => {
      const nx = knobX / radius;
      const ny = -knobY / radius;
      const linear =
        Math.abs(ny) > deadzone ? parseFloat((ny * speed).toFixed(2)) : 0;
      const angular =
        Math.abs(nx) > deadzone ? parseFloat((-nx * speed).toFixed(2)) : 0;
      sendCmd("drive", { linear, angular });
    }, sendInterval);
  }

  function stopSending() {
    if (intervalId) {
      clearInterval(intervalId);
      intervalId = null;
    }
    sendCmd("drive", { linear: 0, angular: 0 });
    knobX = 0;
    knobY = 0;
  }

  function getCenter(el: HTMLElement) {
    const r = el.getBoundingClientRect();
    return { cx: r.left + r.width / 2, cy: r.top + r.height / 2 };
  }

  function onPointerDown(e: PointerEvent) {
    active = true;
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    startSending();
    onPointerMove(e);
  }

  function onPointerMove(e: PointerEvent) {
    if (!active || !joystickEl) return;
    const { cx, cy } = getCenter(joystickEl);
    const dx = clamp(e.clientX - cx, -radius, radius);
    const dy = clamp(e.clientY - cy, -radius, radius);
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist > radius) {
      const s = radius / dist;
      knobX = dx * s;
      knobY = dy * s;
    } else {
      knobX = dx;
      knobY = dy;
    }
  }

  function onPointerUp() {
    active = false;
    stopSending();
  }

  function closeJoystick() {
    onPointerUp();
    joystickOpen.set(false);
  }

  function toggleJoystick() {
    if ($joystickOpen) {
      closeJoystick();
      return;
    }
    openJoystick();
  }

  $: speedLabel = `${Math.round(speed * 100)}%`;
  $: if ($joystickOpen && !positionInitialized) {
    placeJoystickPanel();
  }
  $: if (!$joystickOpen) {
    positionInitialized = false;
  }
</script>

<section class="panel">
  <span class="label">Steuerung</span>

  <button
    type="button"
    class="joystick-toggle"
    class:active={$joystickOpen}
    on:click={toggleJoystick}
  >
    {#if $joystickOpen}
      Joystick ausblenden
    {:else}
      Joystick einblenden
    {/if}
  </button>

  <div class="actions">
    {#each controls as control}
      <button
        type="button"
        class={control.accent}
        disabled={
          !$connection.connected || (control.cmd === "start" && !startAllowed)
        }
        title={control.cmd === "start" && !startAllowed ? startHint : ""}
        on:click={() => control.cmd === "start"
          ? sendCmd("start", missionId ? { missionId } : {})
          : sendCmd(control.cmd)}
      >
        {control.label}
      </button>
    {/each}
  </div>

  {#if !$connection.connected}
    <div class="hint hint-warning">Steuerung erst nach Verbindungsaufbau verfügbar.</div>
  {:else if !startAllowed && startHint}
    <div class="hint hint-warning">{startHint}</div>
  {/if}
</section>

{#if $joystickOpen}
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="dialog" style="left: {panelX}px; top: {panelY}px;">
    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <div
      class="dialog-head"
      on:pointerdown={onPanelPointerDown}
      on:pointermove={onPanelPointerMove}
      on:pointerup={onPanelPointerUp}
    >
      <span>Joystick</span>
      <button
        type="button"
        class="close-btn"
        on:pointerdown|stopPropagation
        on:click={closeJoystick}>✕</button
      >
    </div>

    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <div
      class="pad"
      bind:this={joystickEl}
      on:pointerdown={onPointerDown}
      on:pointermove={onPointerMove}
      on:pointerup={onPointerUp}
      on:pointerleave={onPointerUp}
    >
      <div class="crosshair h"></div>
      <div class="crosshair v"></div>
      <div
        class="knob"
        class:active
        style="transform: translate({knobX}px, {knobY}px)"
      ></div>
    </div>

    <div class="speed-row">
      <span class="speed-label">Geschwindigkeit</span>
      <span class="speed-val">{speedLabel}</span>
    </div>
    <input
      type="range"
      class="speed-slider"
      min="0.1"
      max="1"
      step="0.05"
      bind:value={speed}
    />
  </div>
{/if}

<style>
  .panel {
    display: grid;
    gap: 0.4rem;
    padding: 0.58rem 0.68rem;
    border-top: 1px solid #0f1829;
  }

  .label {
    color: #7a8da8;
    font-size: 0.59rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .actions {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 0.36rem;
  }

  button {
    padding: 0.48rem 0.4rem;
    border-radius: 0.45rem;
    cursor: pointer;
    font-weight: 700;
    font-size: 0.64rem;
    border: 1px solid transparent;
    background: #0a1020;
    color: #94a3b8;
  }

  button:disabled {
    cursor: not-allowed;
    opacity: 0.45;
  }

  .start {
    border-color: #2563eb;
    background: #0c1a3a;
    color: #93c5fd;
  }
  .stop {
    border-color: #475569;
    background: #0f1829;
    color: #94a3b8;
  }
  .dock {
    border-color: #d97706;
    background: #1c1200;
    color: #fbbf24;
  }

  .joystick-toggle {
    width: 100%;
    padding: 0.62rem 0.7rem;
    border-color: #2563eb;
    background: linear-gradient(135deg, #102448, #16346d);
    color: #dbeafe;
    font-size: 0.7rem;
    letter-spacing: 0.02em;
  }

  .joystick-toggle:hover:not(:disabled) {
    background: linear-gradient(135deg, #14315f, #1d4d96);
    border-color: #60a5fa;
  }

  .joystick-toggle.active {
    border-color: #22c55e;
    background: linear-gradient(135deg, #0b3320, #17643b);
    color: #dcfce7;
  }

  .hint {
    font-size: 0.64rem;
    line-height: 1.4;
    border-radius: 0.45rem;
    padding: 0.45rem 0.55rem;
  }

  .hint-warning {
    color: #fbbf24;
    background: rgba(40, 24, 0, 0.7);
    border: 1px solid #92400e;
  }

  /* Floating panel */
  .dialog {
    position: fixed;
    z-index: 100;
    background: #0a1020;
    border: 1px solid #1e3a5f;
    border-radius: 0.85rem;
    padding: 0.75rem;
    display: grid;
    gap: 0.65rem;
    width: 210px;
    box-shadow: 0 16px 40px rgba(0, 0, 0, 0.5);
  }

  .dialog-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    color: #93c5fd;
    font-size: 0.78rem;
    font-weight: 600;
    cursor: grab;
    user-select: none;
  }

  .dialog-head:active {
    cursor: grabbing;
  }

  .close-btn {
    background: transparent;
    border: none;
    color: #475569;
    font-size: 0.9rem;
    cursor: pointer;
    padding: 0.1rem 0.25rem;
    line-height: 1;
  }

  .close-btn:hover {
    color: #94a3b8;
  }

  .pad {
    position: relative;
    width: 110px;
    height: 110px;
    border-radius: 50%;
    background: #060c17;
    border: 2px solid #1e3a5f;
    cursor: grab;
    touch-action: none;
    user-select: none;
    display: flex;
    align-items: center;
    justify-content: center;
    margin: 0 auto;
  }

  .crosshair {
    position: absolute;
    background: rgba(30, 58, 95, 0.5);
  }

  .crosshair.h {
    width: 100%;
    height: 1px;
  }
  .crosshair.v {
    height: 100%;
    width: 1px;
  }

  .knob {
    position: absolute;
    width: 34px;
    height: 34px;
    border-radius: 50%;
    background: #1e3a5f;
    border: 2px solid #3b82f6;
    transition: transform 0.05s ease;
    pointer-events: none;
  }

  .knob.active {
    background: #1d4ed8;
    border-color: #60a5fa;
    transition: none;
  }

  .speed-row {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
  }

  .speed-label {
    font-size: 0.6rem;
    color: #475569;
    text-transform: uppercase;
    letter-spacing: 0.07em;
  }

  .speed-val {
    font-size: 0.7rem;
    font-weight: 600;
    color: #60a5fa;
    font-family: monospace;
  }

  .speed-slider {
    width: 100%;
    accent-color: #2563eb;
    cursor: pointer;
    margin: 0;
  }
</style>
