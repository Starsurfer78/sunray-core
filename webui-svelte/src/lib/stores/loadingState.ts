import { writable } from "svelte/store";

export type OperationType =
  | "save-map"
  | "load-map"
  | "save-mission"
  | "load-mission"
  | "export-map"
  | "import-map"
  | "fetch-telemetry"
  | "send-command"
  | string;

interface LoadingOperation {
  type: OperationType;
  label: string;
  progress?: number;
}

interface LoadingState {
  operations: Map<OperationType, LoadingOperation>;
}

function createLoadingStore() {
  const { subscribe, update } = writable<LoadingState>({
    operations: new Map(),
  });

  function start(type: OperationType, label: string) {
    update((state) => {
      state.operations.set(type, { type, label, progress: undefined });
      return { operations: new Map(state.operations) };
    });
  }

  function updateProgress(type: OperationType, progress: number) {
    update((state) => {
      const op = state.operations.get(type);
      if (op) {
        op.progress = Math.min(100, Math.max(0, progress));
      }
      return { operations: new Map(state.operations) };
    });
  }

  function finish(type: OperationType) {
    update((state) => {
      state.operations.delete(type);
      return { operations: new Map(state.operations) };
    });
  }

  function isLoading(type?: OperationType): boolean {
    let isLoadingValue = false;

    const unsubscribe = subscribe((state) => {
      if (type) {
        isLoadingValue = state.operations.has(type);
      } else {
        isLoadingValue = state.operations.size > 0;
      }
    });

    unsubscribe();
    return isLoadingValue;
  }

  function getOperation(type: OperationType): LoadingOperation | undefined {
    let op: LoadingOperation | undefined;
    const unsubscribe = subscribe((state) => {
      op = state.operations.get(type);
    });
    unsubscribe();
    return op;
  }

  return {
    subscribe,
    start,
    updateProgress,
    finish,
    isLoading,
    getOperation,
  };
}

export const loadingState = createLoadingStore();

export async function withLoading<T>(
  type: OperationType,
  label: string,
  operation: () => Promise<T>,
): Promise<T> {
  loadingState.start(type, label);
  try {
    const result = await operation();
    loadingState.finish(type);
    return result;
  } catch (error) {
    loadingState.finish(type);
    throw error;
  }
}
