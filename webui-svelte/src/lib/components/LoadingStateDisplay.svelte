<script lang="ts">
  import { fade } from 'svelte/transition'
  import { loadingState } from '../stores/loadingState'
</script>

<div class="loading-bar-container">
  {#each [...$loadingState.operations.values()] as operation (operation.type)}
    <div class="loading-bar" in:fade={{ duration: 150 }} out:fade={{ duration: 150 }}>
      <div class="loading-bar-progress">
        <div class="loading-bar-fill" style="width: {operation.progress ?? 0}%"></div>
      </div>
      <span class="loading-bar-label">{operation.label}</span>
    </div>
  {/each}
</div>

<style>
  .loading-bar-container {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    z-index: 9998;
    display: flex;
    flex-direction: column;
    gap: 0;
  }

  .loading-bar {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    padding: 0.5rem 1rem;
    background: rgba(10, 15, 26, 0.95);
    border-bottom: 1px solid #1e3a5f;
    font-size: 0.85rem;
    color: #94a3b8;
  }

  .loading-bar-progress {
    flex: 1;
    height: 3px;
    background: rgba(30, 58, 95, 0.5);
    border-radius: 2px;
    overflow: hidden;
  }

  .loading-bar-fill {
    height: 100%;
    background: linear-gradient(90deg, #60a5fa, #3b82f6);
    transition: width 0.3s ease;
    border-radius: 2px;
  }

  .loading-bar-label {
    white-space: nowrap;
    font-size: 0.8rem;
  }

  @keyframes pulse {
    0%, 100% {
      opacity: 1;
    }
    50% {
      opacity: 0.6;
    }
  }

  .loading-bar-label::after {
    content: '';
    display: inline-block;
    width: 4px;
    height: 4px;
    margin-left: 0.4rem;
    background: #60a5fa;
    border-radius: 50%;
    animation: pulse 1.2s ease-in-out infinite;
  }
</style>
