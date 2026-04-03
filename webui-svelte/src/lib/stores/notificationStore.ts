import { writable } from "svelte/store";

export type NotificationTone = "success" | "error" | "warning" | "info";

export interface Notification {
  id: string;
  message: string;
  tone: NotificationTone;
  persistent: boolean;
  action?: {
    label: string;
    handler: () => void;
  };
  createdAt: number;
}

interface NotificationStore {
  notifications: Notification[];
}

const initialState: NotificationStore = {
  notifications: [],
};

function createNotificationStore() {
  const { subscribe, set, update } = writable<NotificationStore>(initialState);

  let idCounter = 0;
  const timers = new Map<string, ReturnType<typeof setTimeout>>();

  function add(
    message: string,
    tone: NotificationTone = "info",
    options: {
      persistent?: boolean;
      action?: { label: string; handler: () => void };
      autoCloseDuration?: number;
    } = {},
  ) {
    const id = `notification-${++idCounter}`;
    const {
      persistent = false,
      action = undefined,
      autoCloseDuration = tone === "error" ? 6000 : 4000,
    } = options;

    const notification: Notification = {
      id,
      message,
      tone,
      persistent,
      action,
      createdAt: Date.now(),
    };

    update((state) => ({
      ...state,
      notifications: [...state.notifications, notification],
    }));

    if (!persistent) {
      const timer = setTimeout(() => {
        remove(id);
      }, autoCloseDuration);
      timers.set(id, timer);
    }

    return id;
  }

  function remove(id: string) {
    const timer = timers.get(id);
    if (timer) {
      clearTimeout(timer);
      timers.delete(id);
    }

    update((state) => ({
      ...state,
      notifications: state.notifications.filter((n) => n.id !== id),
    }));
  }

  function clear() {
    timers.forEach((timer) => clearTimeout(timer));
    timers.clear();
    set(initialState);
  }

  return {
    subscribe,
    add,
    remove,
    clear,
  };
}

export const notifications = createNotificationStore();

export const toast = {
  success: (message: string, duration?: number) =>
    notifications.add(message, "success", { autoCloseDuration: duration }),

  error: (
    message: string,
    duration?: number,
    action?: { label: string; handler: () => void },
  ) =>
    notifications.add(message, "error", {
      persistent: true,
      autoCloseDuration: duration,
      action,
    }),

  warning: (message: string, duration?: number) =>
    notifications.add(message, "warning", { autoCloseDuration: duration }),

  info: (message: string, duration?: number) =>
    notifications.add(message, "info", { autoCloseDuration: duration }),
};
