import { writable } from "svelte/store";

export interface HistoryEntry<T> {
  state: T;
  description: string;
  timestamp: number;
}

interface HistoryState<T> {
  past: HistoryEntry<T>[];
  present: HistoryEntry<T>;
  future: HistoryEntry<T>[];
}

export function createUndoRedo<T>(initialState: T, maxHistory = 20) {
  let state: HistoryState<T> = {
    past: [],
    present: {
      state: initialState,
      description: "Initial state",
      timestamp: Date.now(),
    },
    future: [],
  };

  const { subscribe, set } = writable<HistoryState<T>>(state);

  function push(newState: T, description: string) {
    state = {
      past: [...state.past.slice(-maxHistory + 1), state.present],
      present: {
        state: newState,
        description,
        timestamp: Date.now(),
      },
      future: [],
    };
    set(state);
  }

  function undo() {
    if (state.past.length === 0) return;

    const newPresent = state.past[state.past.length - 1];
    const newPast = state.past.slice(0, -1);

    state = {
      past: newPast,
      present: newPresent,
      future: [state.present, ...state.future],
    };
    set(state);
  }

  function redo() {
    if (state.future.length === 0) return;

    const newPresent = state.future[0];
    const newFuture = state.future.slice(1);

    state = {
      past: [...state.past, state.present],
      present: newPresent,
      future: newFuture,
    };
    set(state);
  }

  function canUndo() {
    return state.past.length > 0;
  }

  function canRedo() {
    return state.future.length > 0;
  }

  function getCurrentState(): T {
    return state.present.state;
  }

  function getHistory() {
    return state;
  }

  function clear() {
    state = {
      past: [],
      present: {
        state: initialState,
        description: "Reset",
        timestamp: Date.now(),
      },
      future: [],
    };
    set(state);
  }

  return {
    subscribe,
    push,
    undo,
    redo,
    canUndo,
    canRedo,
    getCurrentState,
    getHistory,
    clear,
  };
}
