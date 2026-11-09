import { useCallback, useEffect, useState } from 'react';
import type {
  AppInfo,
  BookmarkItem,
  BrowserState,
  CookieMode,
  DownloadItem,
  HistoryItem,
  MediaItem,
  MethodName,
  MethodPayload,
  MethodResult,
  PrivacyState,
  SitePermissionItem,
  VpnConfigItem,
  VpnTunnelStatus,
} from '@nexus/ipc-contracts';
import { describeError, getDefaultIpcClient } from '../lib/ipc';

const EMPTY_BROWSER_STATE: BrowserState = {
  tabs: [],
  activeTabId: 0,
  dashboardOpen: false,
  activePanel: 'none',
};

const EMPTY_PRIVACY_STATE: PrivacyState = {
  cookieMode: 'strict',
  blockedTrackers: 0,
  blockedCookies: 0,
  deniedPermissions: 0,
  blockedDownloads: 0,
  strippedReferrers: 0,
  blockedPopups: 0,
  blockedNavigations: 0,
  deletedCookies: 0,
};

const INITIAL_VPN_STATUS: VpnTunnelStatus = {
  status: 'disconnected',
  configName: '',
  endpoint: '',
  interfaceIp: '',
  dns: '',
  uptimeSeconds: 0,
  bytesSent: 0,
  bytesReceived: 0,
  killSwitchEnabled: true,
  dnsLeakProtection: true,
  errorMessage: '',
};

export type ActivePanel =
  | 'none'
  | 'privacy'
  | 'history'
  | 'bookmarks'
  | 'downloads'
  | 'media'
  | 'vpn'
  | 'settings';

export interface NexusModel {
  ready: boolean;
  fatalError: string | null;
  appInfo: AppInfo | null;
  browser: BrowserState;
  privacy: PrivacyState;
  downloads: DownloadItem[];
  mediaItems: MediaItem[];
  vpnStatus: VpnTunnelStatus;
  bookmarks: BookmarkItem[];
  focusSignal: number;
  navigate: (input: string) => Promise<void>;
  createTab: (url?: string) => Promise<void>;
  closeTab: (tabId: number) => Promise<void>;
  activateTab: (tabId: number) => Promise<void>;
  duplicateTab: (tabId?: number) => Promise<number>;
  restoreTab: () => Promise<boolean>;
  goBack: () => Promise<void>;
  goForward: () => Promise<void>;
  reload: () => Promise<void>;
  stop: () => Promise<void>;
  zoomIn: () => Promise<number>;
  zoomOut: () => Promise<number>;
  resetZoom: () => Promise<number>;
  findInPage: (query: string, forward?: boolean, findNext?: boolean) => Promise<void>;
  stopFinding: (clearSelection?: boolean) => Promise<void>;
  setDashboardOpen: (open: boolean) => Promise<void>;
  setActivePanel: (panel: ActivePanel) => Promise<void>;
  focusAddress: () => Promise<void>;
  setCookieMode: (mode: CookieMode) => Promise<void>;
  clearCookies: () => Promise<void>;
  getHistory: (limit?: number, query?: string) => Promise<HistoryItem[]>;
  autocompleteHistory: (prefix: string, limit?: number) => Promise<HistoryItem[]>;
  deleteHistoryItem: (id: number) => Promise<void>;
  clearHistory: (olderThanMs?: number) => Promise<void>;
  getBookmarks: () => Promise<BookmarkItem[]>;
  addBookmark: (url: string, title: string) => Promise<BookmarkItem>;
  removeBookmark: (url: string) => Promise<void>;
  isBookmarked: (url: string) => Promise<boolean>;
  getDownloads: (limit?: number) => Promise<DownloadItem[]>;
  cancelDownload: (guid: string) => Promise<void>;
  openDownloadFile: (guid: string) => Promise<void>;
  openDownloadFolder: (guid: string) => Promise<void>;
  getMedia: (sourceUrl?: string, limit?: number) => Promise<MediaItem[]>;
  downloadMedia: (url: string, title?: string) => Promise<boolean>;
  clearMedia: () => Promise<void>;
  getVpnStatus: () => Promise<VpnTunnelStatus>;
  getVpnConfigs: () => Promise<VpnConfigItem[]>;
  importVpnConfig: (name: string, content: string) => Promise<boolean>;
  connectVpn: (name: string, content?: string) => Promise<VpnTunnelStatus>;
  disconnectVpn: () => Promise<VpnTunnelStatus>;
  deleteVpnConfig: (id: number) => Promise<void>;
  setVpnKillSwitch: (enabled: boolean) => Promise<void>;
  getPermissions: () => Promise<SitePermissionItem[]>;
  setPermission: (origin: string, permission: string, state: 'allow' | 'deny' | 'prompt') => Promise<void>;
  clearPermissions: (origin?: string) => Promise<void>;
  getSetting: (key: string, defaultValue?: string) => Promise<string>;
  setSetting: (key: string, value: string) => Promise<void>;
  reloadShell: () => void;
}

export function useNexus(): NexusModel {
  const client = getDefaultIpcClient();

  const [ready, setReady] = useState(false);
  const [fatalError, setFatalError] = useState<string | null>(null);
  const [appInfo, setAppInfo] = useState<AppInfo | null>(null);
  const [browser, setBrowser] = useState<BrowserState>(EMPTY_BROWSER_STATE);
  const [privacy, setPrivacy] = useState<PrivacyState>(EMPTY_PRIVACY_STATE);
  const [downloads, setDownloads] = useState<DownloadItem[]>([]);
  const [mediaItems, setMediaItems] = useState<MediaItem[]>([]);
  const [vpnStatus, setVpnStatus] = useState<VpnTunnelStatus>(INITIAL_VPN_STATUS);
  const [bookmarks, setBookmarks] = useState<BookmarkItem[]>([]);
  const [focusSignal, setFocusSignal] = useState(0);

  const call = useCallback(
    <M extends MethodName>(
      method: M,
      payload: MethodPayload<M>,
    ): Promise<MethodResult<M>> => {
      if (!client) {
        return Promise.reject(new Error('The native bridge is unavailable.'));
      }
      return client.request(method, payload);
    },
    [client],
  );

  const refreshBookmarks = useCallback(async () => {
    try {
      const res = await call('bookmarks.get', {});
      setBookmarks(res.items);
    } catch {
      // Ignored if bridge is still warming up
    }
  }, [call]);

  useEffect(() => {
    if (!client) {
      setFatalError(
        'The native NEXUS bridge is not available. This interface must run inside the NEXUS desktop application.',
      );
      return;
    }

    let cancelled = false;

    const unsubscribe = [
      client.on('browser.stateChanged', (state) => {
        if (!cancelled) {
          setBrowser(state);
        }
      }),
      client.on('privacy.stateChanged', (state) => {
        if (!cancelled) {
          setPrivacy(state);
        }
      }),
      client.on('downloads.changed', (payload) => {
        if (!cancelled) {
          setDownloads(payload.items);
        }
      }),
      client.on('media.detected', (item) => {
        if (!cancelled) {
          setMediaItems((prev) => {
            if (prev.some((existing) => existing.url === item.url)) {
              return prev;
            }
            return [item, ...prev];
          });
        }
      }),
      client.on('vpn.statusChanged', (status) => {
        if (!cancelled) {
          setVpnStatus(status);
        }
      }),
      client.on('ui.focusAddress', () => {
        if (!cancelled) {
          setFocusSignal((value) => value + 1);
        }
      }),
      client.on('ui.dashboardChanged', ({ open }) => {
        if (!cancelled) {
          setBrowser((previous) => ({
            ...previous,
            dashboardOpen: open,
            activePanel: open ? 'privacy' : 'none',
          }));
        }
      }),
    ];

    void (async () => {
      try {
        const [info, browserState, privacyState, initialDownloads, initialVpn, initialBookmarks] =
          await Promise.all([
            client.request('app.getInfo', {}),
            client.request('browser.getState', {}),
            client.request('privacy.getState', {}),
            client.request('downloads.get', { limit: 50 }).catch(() => ({ items: [] as DownloadItem[] })),
            client.request('vpn.getStatus', {}).catch(() => INITIAL_VPN_STATUS),
            client.request('bookmarks.get', {}).catch(() => ({ items: [] as BookmarkItem[] })),
          ]);

        if (cancelled) {
          return;
        }

        setAppInfo(info);
        setBrowser(browserState);
        setPrivacy(privacyState);
        setDownloads(initialDownloads.items);
        setVpnStatus(initialVpn);
        setBookmarks(initialBookmarks.items);
        setReady(true);
      } catch (error) {
        if (!cancelled) {
          setFatalError(describeError(error));
        }
      }
    })();

    return () => {
      cancelled = true;
      for (const off of unsubscribe) {
        off();
      }
    };
  }, [client]);

  const navigate = useCallback(
    async (input: string): Promise<void> => {
      await call('browser.navigate', { url: input });
    },
    [call],
  );

  const createTab = useCallback(
    async (url?: string): Promise<void> => {
      await call('browser.createTab', { url: url ?? '', activate: true });
      await call('ui.focusAddress', {}).catch(() => undefined);
    },
    [call],
  );

  const closeTab = useCallback(
    async (tabId: number): Promise<void> => {
      await call('browser.closeTab', { tabId });
    },
    [call],
  );

  const activateTab = useCallback(
    async (tabId: number): Promise<void> => {
      await call('browser.activateTab', { tabId });
    },
    [call],
  );

  const duplicateTab = useCallback(
    async (tabId?: number): Promise<number> => {
      const payload = tabId !== undefined ? { tabId } : {};
      const res = await call('browser.duplicateTab', payload);
      return res.tabId;
    },
    [call],
  );

  const restoreTab = useCallback(async (): Promise<boolean> => {
    const res = await call('browser.restoreTab', {});
    return res.restored;
  }, [call]);

  const goBack = useCallback(async (): Promise<void> => {
    await call('browser.goBack', {});
  }, [call]);

  const goForward = useCallback(async (): Promise<void> => {
    await call('browser.goForward', {});
  }, [call]);

  const reload = useCallback(async (): Promise<void> => {
    await call('browser.reload', {});
  }, [call]);

  const stop = useCallback(async (): Promise<void> => {
    await call('browser.stop', {});
  }, [call]);

  const zoomIn = useCallback(async (): Promise<number> => {
    const res = await call('browser.zoomIn', {});
    return res.zoomLevel;
  }, [call]);

  const zoomOut = useCallback(async (): Promise<number> => {
    const res = await call('browser.zoomOut', {});
    return res.zoomLevel;
  }, [call]);

  const resetZoom = useCallback(async (): Promise<number> => {
    const res = await call('browser.resetZoom', {});
    return res.zoomLevel;
  }, [call]);

  const findInPage = useCallback(
    async (query: string, forward = true, findNext = false): Promise<void> => {
      await call('browser.find', { query, forward, findNext });
    },
    [call],
  );

  const stopFinding = useCallback(
    async (clearSelection = true): Promise<void> => {
      await call('browser.stopFinding', { clearSelection });
    },
    [call],
  );

  const setDashboardOpen = useCallback(
    async (open: boolean): Promise<void> => {
      await call('ui.setDashboardOpen', { open });
      setBrowser((prev) => ({
        ...prev,
        dashboardOpen: open,
        activePanel: open ? 'privacy' : 'none',
      }));
    },
    [call],
  );

  const setActivePanel = useCallback(
    async (panel: ActivePanel): Promise<void> => {
      await call('ui.setActivePanel', { panel });
      setBrowser((prev) => ({
        ...prev,
        activePanel: panel,
        dashboardOpen: panel === 'privacy',
      }));
    },
    [call],
  );

  const focusAddress = useCallback(async (): Promise<void> => {
    await call('ui.focusAddress', {});
  }, [call]);

  const setCookieMode = useCallback(
    async (mode: CookieMode): Promise<void> => {
      const state = await call('privacy.setCookieMode', { mode });
      setPrivacy(state);
    },
    [call],
  );

  const clearCookies = useCallback(async (): Promise<void> => {
    await call('privacy.clearCookies', {});
  }, [call]);

  // History
  const getHistory = useCallback(
    async (limit = 100, query = ''): Promise<HistoryItem[]> => {
      const res = await call('history.get', { limit, query });
      return res.items;
    },
    [call],
  );

  const autocompleteHistory = useCallback(
    async (prefix: string, limit = 8): Promise<HistoryItem[]> => {
      const res = await call('history.autocomplete', { prefix, limit });
      return res.items;
    },
    [call],
  );

  const deleteHistoryItem = useCallback(
    async (id: number): Promise<void> => {
      await call('history.delete', { id });
    },
    [call],
  );

  const clearHistory = useCallback(
    async (olderThanMs = 0): Promise<void> => {
      await call('history.clear', { olderThanMs });
    },
    [call],
  );

  // Bookmarks
  const getBookmarks = useCallback(async (): Promise<BookmarkItem[]> => {
    const res = await call('bookmarks.get', {});
    setBookmarks(res.items);
    return res.items;
  }, [call]);

  const addBookmark = useCallback(
    async (url: string, title: string): Promise<BookmarkItem> => {
      const item = await call('bookmarks.add', { url, title });
      await refreshBookmarks();
      return item;
    },
    [call, refreshBookmarks],
  );

  const removeBookmark = useCallback(
    async (url: string): Promise<void> => {
      await call('bookmarks.remove', { url });
      await refreshBookmarks();
    },
    [call, refreshBookmarks],
  );

  const isBookmarked = useCallback(
    async (url: string): Promise<boolean> => {
      const res = await call('bookmarks.isBookmarked', { url });
      return res.bookmarked;
    },
    [call],
  );

  // Downloads
  const getDownloads = useCallback(
    async (limit = 50): Promise<DownloadItem[]> => {
      const res = await call('downloads.get', { limit });
      setDownloads(res.items);
      return res.items;
    },
    [call],
  );

  const cancelDownload = useCallback(
    async (guid: string): Promise<void> => {
      await call('downloads.cancel', { guid });
    },
    [call],
  );

  const openDownloadFile = useCallback(
    async (guid: string): Promise<void> => {
      await call('downloads.openFile', { guid });
    },
    [call],
  );

  const openDownloadFolder = useCallback(
    async (guid: string): Promise<void> => {
      await call('downloads.openFolder', { guid });
    },
    [call],
  );

  // Media
  const getMedia = useCallback(
    async (sourceUrl = '', limit = 50): Promise<MediaItem[]> => {
      const res = await call('media.get', { sourceUrl, limit });
      setMediaItems(res.items);
      return res.items;
    },
    [call],
  );

  const downloadMedia = useCallback(
    async (url: string, title?: string): Promise<boolean> => {
      const payload = title !== undefined ? { url, title } : { url };
      const res = await call('media.download', payload);
      return res.started;
    },
    [call],
  );

  const clearMedia = useCallback(async (): Promise<void> => {
    await call('media.clear', {});
    setMediaItems([]);
  }, [call]);

  // VPN
  const getVpnStatus = useCallback(async (): Promise<VpnTunnelStatus> => {
    const status = await call('vpn.getStatus', {});
    setVpnStatus(status);
    return status;
  }, [call]);

  const getVpnConfigs = useCallback(async (): Promise<VpnConfigItem[]> => {
    const res = await call('vpn.getConfigs', {});
    return res.configs;
  }, [call]);

  const importVpnConfig = useCallback(
    async (name: string, content: string): Promise<boolean> => {
      const res = await call('vpn.importConfig', { name, content });
      return res.success;
    },
    [call],
  );

  const connectVpn = useCallback(
    async (name: string, content?: string): Promise<VpnTunnelStatus> => {
      const payload = content !== undefined ? { name, content } : { name };
      const res = await call('vpn.connect', payload);
      setVpnStatus(res);
      return res;
    },
    [call],
  );

  const disconnectVpn = useCallback(async (): Promise<VpnTunnelStatus> => {
    const res = await call('vpn.disconnect', {});
    setVpnStatus(res);
    return res;
  }, [call]);

  const deleteVpnConfig = useCallback(
    async (id: number): Promise<void> => {
      await call('vpn.deleteConfig', { id });
    },
    [call],
  );

  const setVpnKillSwitch = useCallback(
    async (enabled: boolean): Promise<void> => {
      await call('vpn.setKillSwitch', { enabled });
      setVpnStatus((prev) => ({ ...prev, killSwitchEnabled: enabled }));
    },
    [call],
  );

  // Permissions
  const getPermissions = useCallback(async (): Promise<SitePermissionItem[]> => {
    const res = await call('permissions.getAll', {});
    return res.permissions;
  }, [call]);

  const setPermission = useCallback(
    async (
      origin: string,
      permission: string,
      state: 'allow' | 'deny' | 'prompt',
    ): Promise<void> => {
      await call('permissions.set', { origin, permission, state });
    },
    [call],
  );

  const clearPermissions = useCallback(
    async (origin = ''): Promise<void> => {
      await call('permissions.clear', { origin });
    },
    [call],
  );

  // Settings
  const getSetting = useCallback(
    async (key: string, defaultValue = ''): Promise<string> => {
      const res = await call('settings.get', { key, defaultValue });
      return res.value;
    },
    [call],
  );

  const setSetting = useCallback(
    async (key: string, value: string): Promise<void> => {
      await call('settings.set', { key, value });
    },
    [call],
  );

  const reloadShell = useCallback((): void => {
    window.location.reload();
  }, []);

  return {
    ready,
    fatalError,
    appInfo,
    browser,
    privacy,
    downloads,
    mediaItems,
    vpnStatus,
    bookmarks,
    focusSignal,
    navigate,
    createTab,
    closeTab,
    activateTab,
    duplicateTab,
    restoreTab,
    goBack,
    goForward,
    reload,
    stop,
    zoomIn,
    zoomOut,
    resetZoom,
    findInPage,
    stopFinding,
    setDashboardOpen,
    setActivePanel,
    focusAddress,
    setCookieMode,
    clearCookies,
    getHistory,
    autocompleteHistory,
    deleteHistoryItem,
    clearHistory,
    getBookmarks,
    addBookmark,
    removeBookmark,
    isBookmarked,
    getDownloads,
    cancelDownload,
    openDownloadFile,
    openDownloadFolder,
    getMedia,
    downloadMedia,
    clearMedia,
    getVpnStatus,
    getVpnConfigs,
    importVpnConfig,
    connectVpn,
    disconnectVpn,
    deleteVpnConfig,
    setVpnKillSwitch,
    getPermissions,
    setPermission,
    clearPermissions,
    getSetting,
    setSetting,
    reloadShell,
  };
}
