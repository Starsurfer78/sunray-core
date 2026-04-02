import { writable } from "svelte/store";

type CommandFeedbackState = {
  tone: "error";
  message: string;
};

const { subscribe, set } = writable<CommandFeedbackState | null>(null);

let clearTimer: ReturnType<typeof setTimeout> | null = null;

function resetTimer() {
  if (clearTimer) {
    clearTimeout(clearTimer);
    clearTimer = null;
  }
}

function show(message: string, timeoutMs = 4000) {
  resetTimer();
  set({ tone: "error", message });
  clearTimer = setTimeout(() => {
    clearTimer = null;
    set(null);
  }, timeoutMs);
}

function clear() {
  resetTimer();
  set(null);
}

export const commandFeedback = {
  subscribe,
  show,
  clear,
};
