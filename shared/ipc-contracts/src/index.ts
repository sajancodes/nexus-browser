// Typed wire contract shared by the NEXUS native shell and React renderer.
// The native side validates the same protocol version and method allowlist.

export const IPC_PROTOCOL_VERSION = 1 as const;

export type CookieMode = 'strict' | 'balanced';

export interface AppInfo {
  name: string;
  version: string;
  platform: string;
  cefVersion: string;
  chromiumVersion: string;
  dataDir: string;
  sandbox: boolean;
  milestone: number;
}

export interface BrowserTab {
  id: number;
  title: string;
  url: string;
  loading: boolean;
  canGoBack: boolean;
  canGoForward: boolean;
  errorCode: number;
  errorText: string;
  secure: boolean;
  internal: boolean;
}

export interface BrowserState {
  tabs: BrowserTab[];
  activeTabId: number;
  dashboardOpen: boolean;
}

export interface PrivacyState {
  cookieMode: CookieMode;
  blockedCookies: number;
  deniedPermissions: number;
  blockedDownloads: number;
  strippedReferrers: number;
  blockedPopups: number;
  blockedNavigations: number;
  deletedCookies: number;
}

export interface EmptyPayload {
  [key: string]: never;
}

export interface IpcMethods {
  'app.getInfo': { payload: EmptyPayload; result: AppInfo };
  'browser.getState': { payload: EmptyPayload; result: BrowserState };
  'browser.createTab': {
    payload: { url?: string; activate?: boolean };
    result: { tabId: number; url: string; search: boolean };
  };
  'browser.closeTab': {
    payload: { tabId?: number };
    result: EmptyPayload;
  };
  'browser.activateTab': {
    payload: { tabId: number };
    result: EmptyPayload;
  };
  'browser.navigate': {
    payload: { url: string };
    result: { url: string; search: boolean };
  };
  'browser.goBack': { payload: EmptyPayload; result: EmptyPayload };
  'browser.goForward': { payload: EmptyPayload; result: EmptyPayload };
  'browser.reload': { payload: EmptyPayload; result: EmptyPayload };
  'browser.stop': { payload: EmptyPayload; result: EmptyPayload };
  'ui.setDashboardOpen': {
    payload: { open: boolean };
    result: EmptyPayload;
  };
  'ui.focusAddress': { payload: EmptyPayload; result: EmptyPayload };
  'privacy.getState': { payload: EmptyPayload; result: PrivacyState };
  'privacy.setCookieMode': {
    payload: { mode: CookieMode };
    result: PrivacyState;
  };
  'privacy.clearCookies': {
    payload: EmptyPayload;
    result: { started: boolean };
  };
}

export type MethodName = keyof IpcMethods;
export type MethodPayload<M extends MethodName> = IpcMethods[M]['payload'];
export type MethodResult<M extends MethodName> = IpcMethods[M]['result'];

export interface IpcEvents {
  'browser.stateChanged': BrowserState;
  'privacy.stateChanged': PrivacyState;
  'ui.focusAddress': EmptyPayload;
  'ui.dashboardChanged': { open: boolean };
}

export type EventName = keyof IpcEvents;
export type EventPayload<E extends EventName> = IpcEvents[E];

export interface IpcRequest<M extends MethodName = MethodName> {
  v: typeof IPC_PROTOCOL_VERSION;
  id: string;
  method: M;
  payload: MethodPayload<M>;
}

export interface IpcErrorBody {
  code: string;
  message: string;
}

export interface IpcResponse {
  v: typeof IPC_PROTOCOL_VERSION;
  type: 'response';
  id: string;
  ok: boolean;
  result?: unknown;
  error?: IpcErrorBody;
}

export interface IpcEvent {
  v: typeof IPC_PROTOCOL_VERSION;
  type: 'event';
  event: string;
  payload: unknown;
}

export type IpcMessage = IpcResponse | IpcEvent;

export interface NativeBridge {
  postMessage(json: string): void;
  onMessage: ((message: IpcMessage) => void) | null;
}

export function isIpcResponse(value: unknown): value is IpcResponse {
  if (typeof value !== 'object' || value === null) {
    return false;
  }
  const candidate = value as Record<string, unknown>;
  return (
    candidate.v === IPC_PROTOCOL_VERSION &&
    candidate.type === 'response' &&
    typeof candidate.id === 'string' &&
    typeof candidate.ok === 'boolean'
  );
}

export function isIpcEvent(value: unknown): value is IpcEvent {
  if (typeof value !== 'object' || value === null) {
    return false;
  }
  const candidate = value as Record<string, unknown>;
  return (
    candidate.v === IPC_PROTOCOL_VERSION &&
    candidate.type === 'event' &&
    typeof candidate.event === 'string'
  );
}
