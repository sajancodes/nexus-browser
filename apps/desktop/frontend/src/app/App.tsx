import { useCallback, useEffect, useState } from 'react';
import { TabStrip } from '../browser/TabStrip';
import { Toolbar } from '../browser/Toolbar';
import { BookmarksBar } from '../browser/BookmarksBar';
import { FindBar } from '../browser/FindBar';
import { ShieldIcon } from '../components/icons';
import { useNexus, type ActivePanel } from '../hooks/useNexus';
import { runQuietly } from '../lib/format';
import { PrivacyDashboard } from '../pages/privacy/PrivacyDashboard';
import { DownloadsPanel } from '../pages/downloads/DownloadsPanel';
import { MediaPanel } from '../pages/media/MediaPanel';
import { VpnPanel } from '../pages/vpn/VpnPanel';
import { HistoryPanel } from '../pages/history/HistoryPanel';
import { BookmarksPanel } from '../pages/bookmarks/BookmarksPanel';
import { SettingsPanel } from '../pages/settings/SettingsPanel';

export function App() {
  const {
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
    setActivePanel,
    setCookieMode,
    clearCookies,
    getHistory,
    autocompleteHistory,
    deleteHistoryItem,
    clearHistory,
    addBookmark,
    removeBookmark,
    isBookmarked: checkIsBookmarked,
    cancelDownload,
    openDownloadFile,
    openDownloadFolder,
    downloadMedia,
    clearMedia,
    getVpnConfigs,
    importVpnConfig,
    connectVpn,
    disconnectVpn,
    deleteVpnConfig,
    setVpnKillSwitch,
    getPermissions,
    clearPermissions,
    getSetting,
    setSetting,
    reloadShell,
  } = useNexus();

  const [activeTabBookmarked, setActiveTabBookmarked] = useState(false);
  const [findBarOpen, setFindBarOpen] = useState(false);
  const [showBookmarksBar, setShowBookmarksBar] = useState(true);

  const activeTab = browser.tabs.find((tab) => tab.id === browser.activeTabId);
  const activePanel: ActivePanel = browser.activePanel || (browser.dashboardOpen ? 'privacy' : 'none');

  // Check if current tab is bookmarked
  useEffect(() => {
    if (activeTab && activeTab.url && !activeTab.internal) {
      checkIsBookmarked(activeTab.url)
        .then(setActiveTabBookmarked)
        .catch(() => setActiveTabBookmarked(false));
    } else {
      setActiveTabBookmarked(false);
    }
  }, [activeTab?.url, activeTab?.internal, checkIsBookmarked, bookmarks]);

  // Handle global keyboard shortcuts in frontend
  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      const isCtrl = event.ctrlKey || event.metaKey;
      if (isCtrl && event.shiftKey && event.key.toLowerCase() === 't') {
        event.preventDefault();
        runQuietly(restoreTab(), 'restore closed tab');
      } else if (isCtrl && event.key.toLowerCase() === 'h') {
        event.preventDefault();
        setActivePanel(activePanel === 'history' ? 'none' : 'history');
      } else if (isCtrl && event.key.toLowerCase() === 'j') {
        event.preventDefault();
        setActivePanel(activePanel === 'downloads' ? 'none' : 'downloads');
      } else if (isCtrl && event.key.toLowerCase() === 'b') {
        event.preventDefault();
        setShowBookmarksBar((prev) => !prev);
      } else if (isCtrl && event.key.toLowerCase() === 'f') {
        event.preventDefault();
        setFindBarOpen(true);
      } else if (isCtrl && event.key.toLowerCase() === 'd') {
        event.preventDefault();
        handleToggleBookmark();
      } else if (event.key === 'Escape') {
        if (findBarOpen) {
          event.preventDefault();
          setFindBarOpen(false);
          stopFinding(true);
        } else if (activePanel !== 'none') {
          event.preventDefault();
          setActivePanel('none');
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [activePanel, findBarOpen, activeTab]);

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

  const handleToggleBookmark = useCallback(async () => {
    if (!activeTab || activeTab.internal || !activeTab.url) return;
    if (activeTabBookmarked) {
      await removeBookmark(activeTab.url);
      setActiveTabBookmarked(false);
    } else {
      await addBookmark(activeTab.url, activeTab.title || activeTab.url);
      setActiveTabBookmarked(true);
    }
  }, [activeTab, activeTabBookmarked, addBookmark, removeBookmark]);

  const handleTogglePanel = useCallback(
    (panel: ActivePanel) => {
      setActivePanel(panel);
    },
    [setActivePanel],
  );

  const handleFind = useCallback(
    (query: string, forward = true, findNext = false) => {
      runQuietly(findInPage(query, forward, findNext), 'find in page');
    },
    [findInPage],
  );

  const handleCloseFind = useCallback(() => {
    setFindBarOpen(false);
    runQuietly(stopFinding(true), 'stop finding');
  }, [stopFinding]);

  if (!ready) {
    if (fatalError) {
      return (
        <div className="boot-error" role="alert">
          <ShieldIcon />
          <div className="boot-error-text">
            <strong>NEXUS could not reach its native process.</strong>
            <span>{fatalError}</span>
          </div>
          <button type="button" className="retry-button" onClick={reloadShell}>
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

  const blockedTotal =
    privacy.blockedTrackers +
    privacy.blockedCookies +
    privacy.deniedPermissions +
    privacy.blockedDownloads +
    privacy.strippedReferrers +
    privacy.blockedPopups +
    privacy.blockedNavigations;

  const activeDownloads = downloads.filter((d) => d.state === 'in_progress').length;
  const currentZoom = activeTab?.zoomLevel || 0;

  return (
    <div className="chrome-root">
      <div className="chrome">
        <TabStrip
          tabs={browser.tabs}
          activeTabId={browser.activeTabId}
          blockedTotal={blockedTotal}
          onActivate={handleActivate}
          onClose={handleClose}
          onNewTab={handleNewTab}
          onOpenDashboard={() => handleTogglePanel('privacy')}
        />
        <Toolbar
          activeTab={activeTab}
          focusSignal={focusSignal}
          isBookmarked={activeTabBookmarked}
          zoomLevel={currentZoom}
          mediaCount={mediaItems.length}
          downloadsCount={downloads.length}
          activeDownloadsCount={activeDownloads}
          vpnConnected={vpnStatus.status === 'connected'}
          activePanel={activePanel}
          onNavigate={navigate}
          onBack={handleBack}
          onForward={handleForward}
          onReload={handleReload}
          onStop={handleStop}
          onToggleBookmark={handleToggleBookmark}
          onAutocomplete={autocompleteHistory}
          onZoomIn={zoomIn}
          onZoomOut={zoomOut}
          onResetZoom={resetZoom}
          onToggleFind={() => setFindBarOpen((prev) => !prev)}
          onTogglePanel={handleTogglePanel}
        />
        {showBookmarksBar && (
          <BookmarksBar
            bookmarks={bookmarks}
            onNavigate={navigate}
            onRemoveBookmark={removeBookmark}
            onOpenManager={() => handleTogglePanel('bookmarks')}
          />
        )}
      </div>

      {/* Floating Find in Page Bar */}
      {findBarOpen && (
        <FindBar onFind={handleFind} onClose={handleCloseFind} />
      )}

      {/* Drawers and Overlays */}
      {activePanel === 'privacy' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container full-page" onClick={(e) => e.stopPropagation()}>
            <PrivacyDashboard
              appInfo={appInfo}
              privacy={privacy}
              onClose={() => handleTogglePanel('none')}
              onSetCookieMode={setCookieMode}
              onClearCookies={clearCookies}
            />
          </div>
        </div>
      )}

      {activePanel === 'downloads' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container" onClick={(e) => e.stopPropagation()}>
            <DownloadsPanel
              downloads={downloads}
              onCancel={cancelDownload}
              onOpenFile={openDownloadFile}
              onOpenFolder={openDownloadFolder}
              onClose={() => handleTogglePanel('none')}
            />
          </div>
        </div>
      )}

      {activePanel === 'media' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container" onClick={(e) => e.stopPropagation()}>
            <MediaPanel
              mediaItems={mediaItems}
              onDownload={downloadMedia}
              onClear={clearMedia}
              onClose={() => handleTogglePanel('none')}
            />
          </div>
        </div>
      )}

      {activePanel === 'vpn' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container" onClick={(e) => e.stopPropagation()}>
            <VpnPanel
              status={vpnStatus}
              onConnect={connectVpn}
              onDisconnect={disconnectVpn}
              onGetConfigs={getVpnConfigs}
              onImportConfig={importVpnConfig}
              onDeleteConfig={deleteVpnConfig}
              onSetKillSwitch={setVpnKillSwitch}
              onClose={() => handleTogglePanel('none')}
            />
          </div>
        </div>
      )}

      {activePanel === 'history' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container" onClick={(e) => e.stopPropagation()}>
            <HistoryPanel
              onNavigate={(url) => {
                navigate(url);
                handleTogglePanel('none');
              }}
              onGetHistory={getHistory}
              onDeleteItem={deleteHistoryItem}
              onClearHistory={clearHistory}
              onClose={() => handleTogglePanel('none')}
            />
          </div>
        </div>
      )}

      {activePanel === 'bookmarks' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container" onClick={(e) => e.stopPropagation()}>
            <BookmarksPanel
              bookmarks={bookmarks}
              onNavigate={(url) => {
                navigate(url);
                handleTogglePanel('none');
              }}
              onRemoveBookmark={removeBookmark}
              onAddBookmark={addBookmark}
              onClose={() => handleTogglePanel('none')}
            />
          </div>
        </div>
      )}

      {activePanel === 'settings' && (
        <div className="drawer-overlay" onClick={() => handleTogglePanel('none')}>
          <div className="drawer-container" onClick={(e) => e.stopPropagation()}>
            <SettingsPanel
              appInfo={appInfo}
              privacy={privacy}
              onSetCookieMode={setCookieMode}
              onClearCookies={clearCookies}
              onGetPermissions={getPermissions}
              onClearPermissions={clearPermissions}
              onGetSetting={getSetting}
              onSetSetting={setSetting}
              onClose={() => handleTogglePanel('none')}
            />
          </div>
        </div>
      )}
    </div>
  );
}
