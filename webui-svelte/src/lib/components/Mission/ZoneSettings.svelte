<script lang="ts">
  import { missionStore, type Mission } from '../../stores/missions'
  import type { Zone } from '../../stores/map'

  export let mission: Mission
  export let zone: Zone | null = null

  function effectiveValue<K extends keyof Zone['settings']>(key: K): Zone['settings'][K] | null {
    if (!zone) return null
    const override = mission.overrides[zone.id] ?? {}
    return (override[key as keyof typeof override] ?? zone.settings[key]) as Zone['settings'][K]
  }

  function updateOverride<K extends 'stripWidth' | 'angle' | 'edgeMowing' | 'edgeRounds' | 'speed' | 'pattern'>(
    key: K,
    value: K extends 'edgeMowing' ? boolean : K extends 'pattern' ? 'stripe' | 'spiral' : number,
  ) {
    if (!zone) return
    missionStore.updateMissionZoneOverride(mission.id, zone.id, { [key]: value })
  }

  function clearOverrides() {
    if (!zone) return
    missionStore.clearMissionZoneOverride(mission.id, zone.id)
  }
</script>

{#if zone}
  <section class="settings-shell">
    <div class="header">
      <div>
        <span class="label">Zonen-Einstellungen</span>
        <strong>{zone.settings.name}</strong>
      </div>
      <button type="button" class="reset-btn" on:click={clearOverrides}>Reset</button>
    </div>

    <div class="fields">
      <label>
        <span>Schnittbreite</span>
        <input
          type="number"
          min="0.05"
          max="1"
          step="0.01"
          value={effectiveValue('stripWidth') ?? 0.18}
          on:input={(event) => updateOverride('stripWidth', Number((event.currentTarget as HTMLInputElement).value))}
        />
      </label>

      <label class="angle-field">
        <span>Winkel</span>
        <div class="angle-row">
          <input
            type="range"
            min="0"
            max="179"
            value={effectiveValue('angle') ?? 0}
            on:input={(event) => updateOverride('angle', Number((event.currentTarget as HTMLInputElement).value))}
          />
          <strong>{effectiveValue('angle') ?? 0}°</strong>
        </div>
      </label>

      <label>
        <span>Muster</span>
        <select
          value={effectiveValue('pattern') ?? 'stripe'}
          on:change={(event) => updateOverride('pattern', (event.currentTarget as HTMLSelectElement).value as 'stripe' | 'spiral')}
        >
          <option value="stripe">Streifen</option>
          <option value="spiral">Spirale</option>
        </select>
      </label>

      <label class="toggle-field">
        <span>Randmaehen</span>
        <input
          type="checkbox"
          checked={effectiveValue('edgeMowing') ?? true}
          on:change={(event) => updateOverride('edgeMowing', (event.currentTarget as HTMLInputElement).checked)}
        />
      </label>

      <label>
        <span>Randbahnen</span>
        <input
          type="number"
          min="1"
          max="5"
          step="1"
          disabled={!(effectiveValue('edgeMowing') ?? true)}
          value={effectiveValue('edgeRounds') ?? 1}
          on:input={(event) => updateOverride('edgeRounds', Number((event.currentTarget as HTMLInputElement).value))}
        />
      </label>

      <label>
        <span>Geschwindigkeit</span>
        <input
          type="number"
          min="0.1"
          max="3"
          step="0.1"
          value={effectiveValue('speed') ?? 1}
          on:input={(event) => updateOverride('speed', Number((event.currentTarget as HTMLInputElement).value))}
        />
      </label>
    </div>
  </section>
{/if}

<style>
  .settings-shell {
    display: grid;
    gap: 0.9rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
  }
  .label {
    display: block;
    margin-bottom: 0.2rem;
    color: #7a8da8;
    font-size: 0.76rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }
  .reset-btn {
    padding: 0.45rem 0.75rem;
    border-radius: 0.6rem;
    border: 1px solid #1e3a5f;
    background: #0a1020;
    color: #93c5fd;
    cursor: pointer;
  }
  .fields {
    display: grid;
    grid-template-columns: repeat(6, minmax(0, 1fr));
    gap: 0.8rem;
    align-items: end;
  }
  label {
    display: grid;
    gap: 0.35rem;
    color: #94a3b8;
    font-size: 0.76rem;
  }
  input, select {
    width: 100%;
    padding: 0.45rem 0.65rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.6rem;
    background: #0a1020;
    color: #e2e8f0;
  }
  .angle-field {
    grid-column: span 2;
  }
  .angle-row {
    display: flex;
    align-items: center;
    gap: 0.7rem;
  }
  .angle-row input[type='range'] {
    padding: 0;
  }
  .toggle-field input {
    width: auto;
    justify-self: start;
  }
  @media (max-width: 1100px) {
    .fields {
      grid-template-columns: 1fr 1fr;
    }
    .angle-field {
      grid-column: span 2;
    }
  }
</style>
