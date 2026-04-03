<script lang="ts">
  import { createEventDispatcher } from 'svelte'
  import { missionStore, type Mission } from '../../stores/missions'
  import type { Zone } from '../../stores/map'

  export let mission: Mission
  export let zone: Zone | null = null
  export let color: string = '#a855f7'
  export let showHeader: boolean = true

  const dispatch = createEventDispatcher<{
    close: void
    save: void
  }>()

  function effectiveValue<K extends keyof Zone['settings']>(key: K): Zone['settings'][K] | null {
    if (!zone) return null
    const override = mission.overrides[zone.id] ?? {}
    return (override[key as keyof typeof override] ?? zone.settings[key]) as Zone['settings'][K]
  }

  function updateOverride<K extends 'stripWidth' | 'angle' | 'edgeMowing' | 'edgeRounds' | 'speed'>(
    key: K,
    value: K extends 'edgeMowing' ? boolean : number,
  ) {
    if (!zone) return
    missionStore.updateMissionZoneOverride(mission.id, zone.id, { [key]: value })
  }

  $: edgeMowing = (effectiveValue('edgeMowing') ?? true) as boolean
  $: currentAngle = (effectiveValue('angle') ?? 0) as number
</script>

{#if zone}
  <div class="ms-settings">
    {#if showHeader}
      <div class="ms-set-header">
        <span class="ms-set-zone-dot" style="background:{color}"></span>
        <span class="ms-set-title">{zone.settings.name}</span>
        <span class="ms-set-hint">Einstellungen gelten für diese Zone in der aktuellen Mission</span>
        <button type="button" class="ms-set-close" on:click={() => dispatch('close')}>✕</button>
      </div>
    {/if}
    <div class="ms-set-body">

      <div class="ms-set-field">
        <span class="ms-set-lbl">Schnittbreite</span>
        <input
          class="ms-set-in"
          type="number"
          min="0.05"
          max="1"
          step="0.01"
          style="width:76px"
          value={effectiveValue('stripWidth') ?? 0.18}
          on:input={(e) => updateOverride('stripWidth', Number((e.currentTarget as HTMLInputElement).value))}
        />
      </div>

      <div class="ms-set-vdiv"></div>

      <div class="ms-set-field">
        <span class="ms-set-lbl">Winkel</span>
        <div class="ms-ang-row">
          <input
            type="range"
            min="0"
            max="179"
            value={currentAngle}
            on:input={(e) => updateOverride('angle', Number((e.currentTarget as HTMLInputElement).value))}
          />
          <span class="ms-ang-val">{currentAngle}°</span>
        </div>
      </div>

      <div class="ms-set-vdiv"></div>

      <div class="ms-set-field">
        <span class="ms-set-lbl">Randmähen</span>
        <div class="ms-toggle-row">
          <label class="ms-toggle">
            <input
              type="checkbox"
              checked={edgeMowing}
              on:change={(e) => updateOverride('edgeMowing', (e.currentTarget as HTMLInputElement).checked)}
            />
            <span class="ms-toggle-slider"></span>
          </label>
          <span class="ms-toggle-lbl">{edgeMowing ? 'An' : 'Aus'}</span>
        </div>
      </div>

      <div class="ms-set-field">
        <span class="ms-set-lbl">Randbahnen</span>
        <input
          class="ms-set-in"
          type="number"
          min="1"
          max="5"
          step="1"
          style="width:60px"
          disabled={!edgeMowing}
          value={effectiveValue('edgeRounds') ?? 1}
          on:input={(e) => updateOverride('edgeRounds', Number((e.currentTarget as HTMLInputElement).value))}
        />
      </div>

      <div class="ms-set-vdiv"></div>

      <div class="ms-set-field">
        <span class="ms-set-lbl">Geschwindigkeit</span>
        <input
          class="ms-set-in"
          type="number"
          min="0.1"
          max="3"
          step="0.1"
          style="width:76px"
          value={effectiveValue('speed') ?? 1}
          on:input={(e) => updateOverride('speed', Number((e.currentTarget as HTMLInputElement).value))}
        />
      </div>

      <div class="ms-set-vdiv"></div>

    </div>
    <div class="ms-set-actions">
      <button type="button" class="ms-set-action primary" on:click={() => dispatch('save')}>Speichern</button>
    </div>
  </div>
{/if}

<style>
  .ms-settings {
    flex-shrink: 0;
    background: #0a1020;
    border-top: 1px solid #1e3a5f;
    height: 108px;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  .ms-set-header {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 7px 14px;
    border-bottom: 1px solid #1e3a5f;
    flex-shrink: 0;
  }

  .ms-set-zone-dot {
    width: 9px;
    height: 9px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .ms-set-title {
    font-size: 12px;
    font-weight: 500;
    color: #e2e8f0;
    flex: 1;
  }

  .ms-set-hint {
    font-size: 11px;
    color: #475569;
  }

  .ms-set-close {
    background: transparent;
    border: none;
    color: #475569;
    cursor: pointer;
    font-size: 16px;
    padding: 0 2px;
    line-height: 1;
  }

  .ms-set-close:hover { color: #94a3b8; }

  .ms-set-body {
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 9px 14px;
    overflow-x: auto;
    flex: 1;
  }

  .ms-set-field {
    display: flex;
    flex-direction: column;
    gap: 3px;
    flex-shrink: 0;
  }

  .ms-set-lbl {
    font-size: 10px;
    color: #475569;
    white-space: nowrap;
  }

  .ms-set-in {
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #e2e8f0;
    border-radius: 5px;
    padding: 4px 8px;
    font-size: 12px;
    width: 80px;
  }

  .ms-set-in:focus { outline: none; border-color: #2563eb; }
  .ms-set-in:disabled { opacity: 0.45; cursor: not-allowed; }

  .ms-set-vdiv {
    width: 1px;
    height: 46px;
    background: #1e3a5f;
    flex-shrink: 0;
  }

  .ms-ang-row {
    display: flex;
    align-items: center;
    gap: 5px;
    width: 130px;
  }

  .ms-ang-row input[type='range'] {
    flex: 1;
    accent-color: #60a5fa;
    height: 3px;
    cursor: pointer;
    padding: 0;
  }

  .ms-ang-val {
    font-size: 11px;
    color: #60a5fa;
    font-family: monospace;
    width: 30px;
    text-align: right;
    flex-shrink: 0;
  }

  .ms-toggle-row {
    display: flex;
    align-items: center;
    gap: 6px;
    margin-top: 6px;
  }

  .ms-toggle {
    position: relative;
    width: 30px;
    height: 17px;
    flex-shrink: 0;
  }

  .ms-toggle input {
    opacity: 0;
    width: 0;
    height: 0;
  }

  .ms-toggle-slider {
    position: absolute;
    inset: 0;
    background: #1e3a5f;
    border-radius: 17px;
    cursor: pointer;
    transition: 0.2s;
  }

  .ms-toggle-slider::before {
    content: '';
    position: absolute;
    height: 11px;
    width: 11px;
    left: 3px;
    top: 3px;
    background: #475569;
    border-radius: 50%;
    transition: 0.2s;
  }

  .ms-toggle input:checked + .ms-toggle-slider { background: #2563eb; }

  .ms-toggle input:checked + .ms-toggle-slider::before {
    transform: translateX(13px);
    background: #e2e8f0;
  }

  .ms-toggle-lbl {
    font-size: 12px;
    color: #94a3b8;
  }

  .ms-set-actions {
    display: flex;
    justify-content: flex-end;
    gap: 8px;
    padding: 8px 14px;
    border-top: 1px solid #1e3a5f;
  }

  .ms-set-action {
    padding: 4px 10px;
    border-radius: 5px;
    border: 1px solid #1e3a5f;
    background: #0f1829;
    color: #cbd5e1;
    font-size: 11px;
    cursor: pointer;
  }

  .ms-set-action.primary {
    border-color: #2563eb;
    color: #93c5fd;
  }
</style>
