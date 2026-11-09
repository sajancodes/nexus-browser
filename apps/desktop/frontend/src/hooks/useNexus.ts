import { useCallback, useEffect, useState } from 'react';
import type {
  AppInfo,
  BrowserState,
  CookieMode,
  MethodName,
  MethodPayload,
  MethodResult,
  PrivacyState,
} from '@nexus/ipc-contracts';
import { describeError, getDefaultIpcClient } from '../lib/ipc';

const EMPTY_BROWSER_STATE: BrowserState = {
  tabs: [],
  activeTabId: 0,
  dashboardOpen: false,
};

const EMPTY_PRIVACY_STATE: PrivacyState = {
  cookieMode: 'strict',
  blockedCookies: 0,
  deniedPermissions: 0,
  blockedDownloads: 0,
  strippedReferrers: 0,
  blockedPopups: 0,
  blockedNavigations: 0,
  deletedCookies: 0,
};

export interface NexusModel {
  ready: boolean;
  fatalError: string | null;
  appInfo: AppInfo | null;
  browser: BrowserState;
  privacy: PrivacyState;
  focusSignal: number;
  navigate: (input: string) => Promise<void>;
  createTab: () => Promise<void>;
  closeTab: (tabId: number) => Promise<void>;
  activateTab: (tabId: number) => Promise<void>;
  goBack: () => Promise<void>;
  goForward: () => Promise<void>;
  reload: () => Promise<void>;
  stop: () => Promise<void>;
  setDashboardOpen: (open: boolean) => Promise<void>;
  focusAddress: () => Promise<void>;
  setCookieMode: (mode: CookieMode) => Promise<void>;
  clearCookies: () => Promise<void>;
  reloadShell: () => void;
}

export function useNexus(): NexusModel {
  const client = getDefaultIpcClient();

  const [ready, setReady] = useState(false);
  const [fatalError, setFatalError] = useState<string | null>(null);
  const [appInfo, setAppInfo] = useState<AppInfo | null>(null);
  const [browser, setBrowser] = useState<BrowserState>(EMPTY_BROWSER_STATE);
  const [privacy, setPrivacy] = useState<PrivacyState>(EMPTY_PRIVACY_STATE);
  const [focusSignal, setFocusSignal] = useState(0);

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
      client.on('ui.focusAddress', () => {
        if (!cancelled) {
          setFocusSignal((value) => value + 1);
        }
      }),
      client.on('ui.dashboardChanged', ({ open }) => {
        if (!cancelled) {
          setBrowser((previous) => ({ ...previous, dashboardOpen: open }));
        }
      }),
    ];

    void (async () => {
      try {
        const [info, browserState, privacyState] = await Promise.all([
          client.request('app.getInfo', {}),
          client.request('browser.getState', {}),
          client.request('privacy.getState', {}),
        ]);
        if (cancelled) {
          return;
        }
        setAppInfo(info);
        setBrowser(browserState);
        setPrivacy(privacyState);
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

  const call = useCallback(
    <M extends MethodName>(
      method: M,
      payload: MethodPayload<M>,
    ): Promise<MethodResult<M>> => {
      if (!client) {
        return Promise.reject(
          new Error('The native bridge is unavailable.'),
        );
      }
      return client.request(method, payload);
    },
    [client],
  );

  const navigate = useCallback(
    async (input: string): Promise<void> => {
      await call('browser.navigate', { url: input });
    },
    [call],
  );

  const createTab = useCallback(async (): Promise<void> => {
    await call('browser.createTab', { activate: true });
    await call('ui.focusAddress', {}).catch(() => undefined);
  }, [call]);

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

  const setDashboardOpen = useCallback(
    async (open: boolean): Promise<void> => {
      await call('ui.setDashboardOpen', { open });
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

  const reloadShell = useCallback((): void => {
    window.location.reload();
  }, []);

  return {
    ready,
    fatalError,
    appInfo,
    browser,
    privacy,
    focusSignal,
    navigate,
    createTab,
    closeTab,
    activateTab,
    goBack,
    goForward,
    reload,
    stop,
    setDashboardOpen,
    focusAddress,
    setCookieMode,
    clearCookies,
    reloadShell,
  };
}
