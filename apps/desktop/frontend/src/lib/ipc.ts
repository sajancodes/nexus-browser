import {
  IPC_PROTOCOL_VERSION,
  isIpcEvent,
  isIpcResponse,
  type EventName,
  type EventPayload,
  type IpcErrorBody,
  type MethodName,
  type MethodPayload,
  type MethodResult,
  type NativeBridge,
} from '@nexus/ipc-contracts';

const DEFAULT_TIMEOUT_MS = 10_000;

interface PendingRequest {
  resolve: (value: unknown) => void;
  reject: (reason: Error) => void;
  timer: number;
}

type EventHandler = (payload: unknown) => void;

export class NexusIpcError extends Error {
  readonly code: string;

  constructor(code: string, message: string) {
    super(message);
    this.name = 'NexusIpcError';
    this.code = code;
  }
}

export function describeError(error: unknown): string {
  if (error instanceof Error && error.message) {
    return error.message;
  }
  return 'The native request failed.';
}

/**
 * Typed request/response client for the native browser process.
 *
 * The native side validates the protocol version, method allowlist, payload
 * types and a request rate limit; this client is the only caller reachable
 * from the trusted nexus://app shell.
 */
export class NexusIpcClient {
  private readonly bridge: NativeBridge;
  private readonly pending = new Map<string, PendingRequest>();
  private readonly handlers = new Map<EventName, Set<EventHandler>>();
  private sequence = 0;
  private closed = false;

  constructor(bridge: NativeBridge) {
    this.bridge = bridge;
    bridge.onMessage = (message) => {
      this.handleMessage(message);
    };
  }

  request<M extends MethodName>(
    method: M,
    payload: MethodPayload<M>,
  ): Promise<MethodResult<M>> {
    if (this.closed) {
      return Promise.reject(
        new NexusIpcError('bridge_closed', 'The native bridge is closed.'),
      );
    }

    const id = this.nextId();
    const promise = new Promise<unknown>((resolve, reject) => {
      const timer = window.setTimeout(() => {
        this.pending.delete(id);
        reject(
          new NexusIpcError(
            'timeout',
            'The native browser process did not answer in time.',
          ),
        );
      }, DEFAULT_TIMEOUT_MS);

      this.pending.set(id, { resolve, reject, timer });
    });

    try {
      this.bridge.postMessage(
        JSON.stringify({
          v: IPC_PROTOCOL_VERSION,
          id,
          method,
          payload,
        }),
      );
    } catch {
      const pending = this.pending.get(id);
      if (pending) {
        window.clearTimeout(pending.timer);
        this.pending.delete(id);
        pending.reject(
          new NexusIpcError(
            'bridge_failure',
            'The native bridge rejected the request.',
          ),
        );
      }
    }

    return promise as Promise<MethodResult<M>>;
  }

  on<E extends EventName>(
    event: E,
    handler: (payload: EventPayload<E>) => void,
  ): () => void {
    let handlers = this.handlers.get(event);
    if (!handlers) {
      handlers = new Set<EventHandler>();
      this.handlers.set(event, handlers);
    }

    const stored = handler as EventHandler;
    handlers.add(stored);

    return () => {
      const current = this.handlers.get(event);
      if (!current) {
        return;
      }
      current.delete(stored);
      if (current.size === 0) {
        this.handlers.delete(event);
      }
    };
  }

  close(): void {
    if (this.closed) {
      return;
    }
    this.closed = true;
    this.bridge.onMessage = null;

    for (const [id, pending] of this.pending) {
      window.clearTimeout(pending.timer);
      pending.reject(
        new NexusIpcError('bridge_closed', 'The native bridge closed.'),
      );
      this.pending.delete(id);
    }
  }

  private handleMessage(message: unknown): void {
    if (isIpcResponse(message)) {
      const pending = this.pending.get(message.id);
      if (!pending) {
        return;
      }
      this.pending.delete(message.id);
      window.clearTimeout(pending.timer);

      if (message.ok) {
        pending.resolve(message.result ?? {});
      } else {
        const body: IpcErrorBody = message.error ?? {
          code: 'unknown',
          message: 'The native request failed.',
        };
        pending.reject(new NexusIpcError(body.code, body.message));
      }
      return;
    }

    if (isIpcEvent(message)) {
      const handlers = this.handlers.get(message.event as EventName);
      if (!handlers) {
        return;
      }
      for (const handler of handlers) {
        try {
          handler(message.payload);
        } catch {
          // A renderer-side handler failure must not interrupt IPC delivery.
        }
      }
    }
  }

  private nextId(): string {
    if (
      typeof crypto !== 'undefined' &&
      typeof crypto.randomUUID === 'function'
    ) {
      return crypto.randomUUID();
    }
    this.sequence += 1;
    return `nexus-${Date.now().toString(36)}-${this.sequence}`;
  }
}

let defaultClient: NexusIpcClient | null = null;

export function getNativeBridge(): NativeBridge | null {
  if (typeof window === 'undefined') {
    return null;
  }
  const bridge = window.__nexusNative;
  if (!bridge || typeof bridge.postMessage !== 'function') {
    return null;
  }
  return bridge;
}

/**
 * One client per shell document. The bridge is injected by the native process
 * before any page script runs, so a missing bridge is a real startup failure,
 * not something to retry silently.
 */
export function getDefaultIpcClient(): NexusIpcClient | null {
  if (defaultClient) {
    return defaultClient;
  }
  const bridge = getNativeBridge();
  if (!bridge) {
    return null;
  }
  defaultClient = new NexusIpcClient(bridge);
  return defaultClient;
}
