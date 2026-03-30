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
    <p>Minimaler V1-Bedienblock fuer Start, Stop und Dock.</p>
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
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
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
    color: #9db3ab;
  }

  .actions {
    display: flex;
    gap: 0.8rem;
    flex-wrap: wrap;
  }

  button {
    min-width: 7rem;
    padding: 0.8rem 1rem;
    border: 0;
    border-radius: 0.9rem;
    cursor: pointer;
    color: #07110f;
    font-weight: 700;
  }

  button:disabled {
    cursor: not-allowed;
    opacity: 0.45;
  }

  .start {
    background: #8fcf72;
  }

  .stop {
    background: #f39b89;
  }

  .dock {
    background: #e0c97f;
  }
</style>
