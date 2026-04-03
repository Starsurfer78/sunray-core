<script lang="ts">
  import { fade, fly } from 'svelte/transition'
  import { notifications, type NotificationTone } from '../stores/notificationStore'

  function getToneStyles(tone: NotificationTone) {
    const styles = {
      success: {
        bg: 'rgba(34, 197, 94, 0.1)',
        border: '#22c55e',
        text: '#86efac',
        icon: '✓',
      },
      error: {
        bg: 'rgba(239, 68, 68, 0.1)',
        border: '#ef4444',
        text: '#fca5a5',
        icon: '!',
      },
      warning: {
        bg: 'rgba(245, 158, 11, 0.1)',
        border: '#f59e0b',
        text: '#fcd34d',
        icon: '⚠',
      },
      info: {
        bg: 'rgba(59, 130, 246, 0.1)',
        border: '#3b82f6',
        text: '#93c5fd',
        icon: 'ℹ',
      },
    }
    return styles[tone] || styles.info
  }
</script>

<div class="toast-container" role="region" aria-live="polite" aria-atomic="true">
  {#each $notifications.notifications as notification (notification.id)}
    <div
      class="toast"
      style="
        --tone-bg: {getToneStyles(notification.tone).bg};
        --tone-border: {getToneStyles(notification.tone).border};
        --tone-text: {getToneStyles(notification.tone).text};
      "
      in:fly={{ x: 220, duration: 200 }}
      out:fade={{ duration: 150 }}
    >
      <div class="toast-icon">{getToneStyles(notification.tone).icon}</div>

      <div class="toast-content">
        <p class="toast-message">{notification.message}</p>
      </div>

      {#if notification.action}
        <button
          class="toast-action"
          on:click={() => {
            notification.action?.handler();
            notifications.remove(notification.id);
          }}
        >
          {notification.action.label}
        </button>
      {/if}

      <button
        class="toast-close"
        on:click={() => notifications.remove(notification.id)}
        aria-label="Schließen"
      >
        ×
      </button>
    </div>
  {/each}
</div>

<style>
  .toast-container {
    position: fixed;
    top: 0;
    right: 0;
    z-index: 9999;
    pointer-events: none;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    padding: 1rem;
    max-width: 420px;
  }

  .toast {
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 1rem;
    border-radius: 0.6rem;
    border: 1px solid var(--tone-border);
    background: var(--tone-bg);
    color: var(--tone-text);
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
    pointer-events: auto;
  }

  .toast-icon {
    flex-shrink: 0;
    width: 24px;
    height: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: 700;
    font-size: 1.2rem;
  }

  .toast-content {
    flex: 1;
  }

  .toast-message {
    margin: 0;
    font-size: 0.95rem;
    line-height: 1.4;
  }

  .toast-action {
    flex-shrink: 0;
    padding: 0.4rem 0.8rem;
    border: 1px solid var(--tone-border);
    border-radius: 0.4rem;
    background: transparent;
    color: var(--tone-text);
    font-weight: 500;
    font-size: 0.85rem;
    cursor: pointer;
    transition: all 0.2s;
  }

  .toast-action:hover {
    background: var(--tone-border);
    color: #000;
  }

  .toast-close {
    flex-shrink: 0;
    width: 24px;
    height: 24px;
    padding: 0;
    border: none;
    background: transparent;
    color: var(--tone-text);
    font-size: 1.5rem;
    line-height: 1;
    cursor: pointer;
    opacity: 0.7;
    transition: opacity 0.2s;
  }

  .toast-close:hover {
    opacity: 1;
  }

  @media (max-width: 640px) {
    .toast-container {
      max-width: calc(100vw - 2rem);
      left: 1rem;
      right: 1rem;
    }

    .toast {
      flex-wrap: wrap;
    }

    .toast-action {
      width: 100%;
      order: 4;
    }
  }
</style>
