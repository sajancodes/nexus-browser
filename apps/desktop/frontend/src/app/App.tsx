import { useCallback } from 'react';
import { TabStrip } from '../browser/TabStrip';
import { Toolbar } from '../browser/Toolbar';
import { ShieldIcon } from '../components/icons';
import { useNexus } from '../hooks/useNexus';
import { runQuietly } from '../lib/format';
import { PrivacyDashboard } from '../pages/privacy/PrivacyDashboard';

export function App() {
  const {
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
    setCookieMode,
    clearCookies,
    reloadShell,
  } = useNexus();

  const activeTab = browser.tabs.find(
    (tab) => tab.id === browser.activeTabId,
  );

  const handleActivate = useCallback(
    (tabId: number) => {
      runQuietly(activateTab(tabId), 'activate tab');
    },
    [activateTab],
  );

  const handleClose = useCallback(
    (tabId: number) => {
      runQuietly(closeTab(tabId), 'close tab');
    },
    [closeTab],
  );

  const handleNewTab = useCallback(() => {
    runQuietly(createTab(), 'new tab');
  }, [createTab]);

  const openDashboard = useCallback(() => {
    runQuietly(setDashboardOpen(true), 'open privacy dashboard');
  }, [setDashboardOpen]);

  const closeDashboard = useCallback(() => {
    runQuietly(setDashboardOpen(false), 'close privacy dashboard');
  }, [setDashboardOpen]);

  const handleBack = useCallback(() => {
    runQuietly(goBack(), 'navigate back');
  }, [goBack]);

  const handleForward = useCallback(() => {
    runQuietly(goForward(), 'navigate forward');
  }, [goForward]);

  const handleReload = useCallback(() => {
    runQuietly(reload(), 'reload');
  }, [reload]);

  const handleStop = useCallback(() => {
    runQuietly(stop(), 'stop loading');
  }, [stop]);

  if (!ready) {
    if (fatalError) {
      return (
        <div className="boot-error" role="alert">
          <ShieldIcon />
          <div className="boot-error-text">
            <strong>NEXUS could not reach its native process.</strong>
            <span>{fatalError}</span>
          </div>
          <button
            type="button"
            className="retry-button"
            onClick={reloadShell}
          >
            Retry
          </button>
        </div>
      );
    }
    return (
      <div className="boot-screen" role="status">
        <span>Starting NEXUS…</span>
      </div>
    );
  }

  if (browser.dashboardOpen) {
    return (
      <PrivacyDashboard
        appInfo={appInfo}
        privacy={privacy}
        onClose={closeDashboard}
        onSetCookieMode={setCookieMode}
        onClearCookies={clearCookies}
      />
    );
  }

  const blockedTotal =
    privacy.blockedCookies +
    privacy.deniedPermissions +
    privacy.blockedDownloads +
    privacy.strippedReferrers +
    privacy.blockedPopups +
    privacy.blockedNavigations;

  return (
    <div className="chrome">
      <TabStrip
        tabs={browser.tabs}
        activeTabId={browser.activeTabId}
        blockedTotal={blockedTotal}
        onActivate={handleActivate}
        onClose={handleClose}
        onNewTab={handleNewTab}
        onOpenDashboard={openDashboard}
      />
      <Toolbar
        activeTab={activeTab}
        focusSignal={focusSignal}
        onNavigate={navigate}
        onBack={handleBack}
        onForward={handleForward}
        onReload={handleReload}
        onStop={handleStop}
      />
    </div>
  );
}
