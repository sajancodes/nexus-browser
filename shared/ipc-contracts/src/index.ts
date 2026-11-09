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
  zoomLevel?: number;
}

export interface BrowserState {
  tabs: BrowserTab[];
  activeTabId: number;
  dashboardOpen: boolean;
  activePanel?: 'none' | 'privacy' | 'history' | 'bookmarks' | 'downloads' | 'media' | 'vpn' | 'settings';
}

export interface PrivacyState {
  cookieMode: CookieMode;
  blockedTrackers: number;
  blockedCookies: number;
  deniedPermissions: number;
  blockedDownloads: number;
  strippedReferrers: number;
  blockedPopups: number;
  blockedNavigations: number;
  deletedCookies: number;
}

export interface HistoryItem {
  id: number;
  url: string;
  title: string;
  visitTime: number;
  visitCount: number;
}

export interface BookmarkItem {
  id: number;
  url: string;
  title: string;
  createdAt: number;
}

export interface DownloadItem {
  id: number;
  guid: string;
  url: string;
  fileName: string;
  filePath: string;
  totalBytes: number;
  receivedBytes: number;
  speed: number;
  percent: number;
  state: 'in_progress' | 'completed' | 'cancelled' | 'failed' | 'paused';
  createdAt: number;
  completedAt: number;
}

export interface MediaItem {
  id: number;
  url: string;
  sourceUrl: string;
  mediaType: 'video' | 'audio' | 'image' | 'stream';
  title: string;
  sizeBytes: number;
  discoveredAt: number;
}

export interface VpnTunnelStatus {
  status: 'disconnected' | 'connecting' | 'connected' | 'disconnecting' | 'error';
  configName: string;
  endpoint: string;
  interfaceIp: string;
  dns: string;
  uptimeSeconds: number;
  bytesSent: number;
  bytesReceived: number;
  killSwitchEnabled: boolean;
  dnsLeakProtection: boolean;
  errorMessage: string;
}

export interface VpnConfigItem {
  id: number;
  name: string;
  content: string;
  interfaceAddress: string;
  endpoint: string;
  createdAt: number;
  isActive: boolean;
}

export interface SitePermissionItem {
  origin: string;
  permission: string;
  state: 'allow' | 'deny' | 'prompt';
  updatedAt: number;
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
  'browser.duplicateTab': {
    payload: { tabId?: number };
    result: { tabId: number };
  };
  'browser.restoreTab': {
    payload: EmptyPayload;
    result: { restored: boolean };
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
  'browser.zoomIn': { payload: EmptyPayload; result: { zoomLevel: number } };
  'browser.zoomOut': { payload: EmptyPayload; result: { zoomLevel: number } };
  'browser.resetZoom': { payload: EmptyPayload; result: { zoomLevel: number } };
  'browser.find': {
    payload: { query: string; forward?: boolean; findNext?: boolean };
    result: EmptyPayload;
  };
  'browser.stopFinding': {
    payload: { clearSelection?: boolean };
    result: EmptyPayload;
  };

  'ui.setDashboardOpen': {
    payload: { open: boolean };
    result: EmptyPayload;
  };
  'ui.setActivePanel': {
    payload: { panel: 'none' | 'privacy' | 'history' | 'bookmarks' | 'downloads' | 'media' | 'vpn' | 'settings' };
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

  // History
  'history.get': {
    payload: { limit?: number; query?: string };
    result: { items: HistoryItem[] };
  };
  'history.autocomplete': {
    payload: { prefix: string; limit?: number };
    result: { items: HistoryItem[] };
  };
  'history.delete': {
    payload: { id: number };
    result: EmptyPayload;
  };
  'history.clear': {
    payload: { olderThanMs?: number };
    result: EmptyPayload;
  };

  // Bookmarks
  'bookmarks.get': {
    payload: EmptyPayload;
    result: { items: BookmarkItem[] };
  };
  'bookmarks.add': {
    payload: { url: string; title: string };
    result: BookmarkItem;
  };
  'bookmarks.remove': {
    payload: { url: string };
    result: EmptyPayload;
  };
  'bookmarks.isBookmarked': {
    payload: { url: string };
    result: { bookmarked: boolean };
  };

  // Downloads
  'downloads.get': {
    payload: { limit?: number };
    result: { items: DownloadItem[] };
  };
  'downloads.cancel': {
    payload: { guid: string };
    result: EmptyPayload;
  };
  'downloads.openFile': {
    payload: { guid: string };
    result: EmptyPayload;
  };
  'downloads.openFolder': {
    payload: { guid: string };
    result: EmptyPayload;
  };

  // Media
  'media.get': {
    payload: { sourceUrl?: string; limit?: number };
    result: { items: MediaItem[] };
  };
  'media.download': {
    payload: { url: string; title?: string };
    result: { started: boolean };
  };
  'media.clear': {
    payload: EmptyPayload;
    result: EmptyPayload;
  };

  // VPN
  'vpn.getStatus': {
    payload: EmptyPayload;
    result: VpnTunnelStatus;
  };
  'vpn.getConfigs': {
    payload: EmptyPayload;
    result: { configs: VpnConfigItem[] };
  };
  'vpn.importConfig': {
    payload: { name: string; content: string };
    result: { success: boolean };
  };
  'vpn.connect': {
    payload: { name: string; content?: string };
    result: VpnTunnelStatus;
  };
  'vpn.disconnect': {
    payload: EmptyPayload;
    result: VpnTunnelStatus;
  };
  'vpn.deleteConfig': {
    payload: { id: number };
    result: EmptyPayload;
  };
  'vpn.setKillSwitch': {
    payload: { enabled: boolean };
    result: EmptyPayload;
  };

  // Permissions
  'permissions.getAll': {
    payload: EmptyPayload;
    result: { permissions: SitePermissionItem[] };
  };
  'permissions.set': {
    payload: { origin: string; permission: string; state: 'allow' | 'deny' | 'prompt' };
    result: EmptyPayload;
  };
  'permissions.clear': {
    payload: { origin?: string };
    result: EmptyPayload;
  };

  // Settings
  'settings.get': {
    payload: { key: string; defaultValue?: string };
    result: { value: string };
  };
  'settings.set': {
    payload: { key: string; value: string };
    result: EmptyPayload;
  };
}

export type MethodName = keyof IpcMethods;
export type MethodPayload<M extends MethodName> = IpcMethods[M]['payload'];
export type MethodResult<M extends MethodName> = IpcMethods[M]['result'];

export interface IpcEvents {
  'browser.stateChanged': BrowserState;
  'privacy.stateChanged': PrivacyState;
  'downloads.changed': { items: DownloadItem[] };
  'media.detected': MediaItem;
  'vpn.statusChanged': VpnTunnelStatus;
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
