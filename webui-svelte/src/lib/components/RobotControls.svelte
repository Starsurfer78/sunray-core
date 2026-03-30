<script lang="ts">
  import { sendCmd } from '../api/websocket'
  import { connection } from '../stores/connection'

  const controls = [
    { label: 'Start', cmd: 'start', accent: 'start' },
    { label: 'Stop', cmd: 'stop', accent: 'stop' },
    { label: 'Dock', cmd: 'dock', accent: 'dock' },
  ] as const
</script>

<section class="panel">
  <header>
    <h2>Steuerung</h2>
    <p>Schnellzugriff fuer Mission, Stopp und Docking.</p>
  </header>

  <div class="actions">
    {#each controls as control}
      <button
        type="button"
        class={control.accent}
        disabled={!$connection.connected}
        on:click={() => sendCmd(control.cmd)}
      >
        {control.label}
      </button>
    {/each}
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1rem;
    border-radius: 0.9rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  header {
    display: grid;
    gap: 0.25rem;
  }

  h2,
  p {
    margin: 0;
  }

  p {
    color: #64748b;
    font-size: 0.84rem;
  }

  .actions {
    display: flex;
    gap: 0.8rem;
    flex-wrap: wrap;
  }

  button {
    min-width: 7rem;
    padding: 0.8rem 1rem;
    border-radius: 0.6rem;
    cursor: pointer;
    font-weight: 700;
    border: 1px solid transparent;
    background: #0a1020;
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
</style>
