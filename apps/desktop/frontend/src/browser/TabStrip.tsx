import type { MouseEvent, KeyboardEvent } from 'react';
import type { BrowserTab } from '@nexus/ipc-contracts';
import {
  displayTitleOfTab,
  formatBadgeCount,
  initialOfUrl,
} from '../lib/format';
import { CloseIcon, PlusIcon, ShieldIcon } from '../components/icons';

interface TabStripProps {
  tabs: BrowserTab[];
  activeTabId: number;
  blockedTotal: number;
  onActivate: (tabId: number) => void;
  onClose: (tabId: number) => void;
  onNewTab: () => void;
  onOpenDashboard: () => void;
}

export function TabStrip({
  tabs,
  activeTabId,
  blockedTotal,
  onActivate,
  onClose,
  onNewTab,
  onOpenDashboard,
}: TabStripProps) {
  const handleAuxClick = (event: MouseEvent<HTMLDivElement>, tabId: number) => {
    if (event.button === 1) {
      event.preventDefault();
      onClose(tabId);
    }
  };

  const handleTabKeyDown = (
    event: KeyboardEvent<HTMLDivElement>,
    tabId: number,
  ) => {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      onActivate(tabId);
    }
  };

  return (
    <div className="tabstrip">
      <div className="tabs" role="tablist" aria-label="Open tabs">
        {tabs.map((tab) => {
          const title = displayTitleOfTab(tab);
          const active = tab.id === activeTabId;
          return (
            <div
              key={tab.id}
              role="tab"
              aria-selected={active}
              tabIndex={active ? 0 : -1}
              className="tab"
              data-active={active ? 'true' : 'false'}
              title={tab.url ? `${title} — ${tab.url}` : title}
              onClick={() => onActivate(tab.id)}
              onAuxClick={(event) => handleAuxClick(event, tab.id)}
              onMouseDown={(event) => {
                if (event.button === 1) {
                  event.preventDefault();
                }
              }}
              onKeyDown={(event) => handleTabKeyDown(event, tab.id)}
            >
              <span className="tab-favicon" aria-hidden="true">
                {tab.internal ? <ShieldIcon /> : initialOfUrl(tab.url)}
              </span>
              <span className="tab-title">{title}</span>
              {tab.loading ? (
                <span className="tab-loading" aria-label="Loading" />
              ) : null}
              <button
                type="button"
                className="tab-close"
                aria-label={`Close ${title}`}
                title="Close tab (Ctrl+W)"
                onClick={(event) => {
                  event.stopPropagation();
                  onClose(tab.id);
                }}
              >
                <CloseIcon />
              </button>
            </div>
          );
        })}
      </div>

      <button
        type="button"
        className="icon-button tab-new"
        onClick={onNewTab}
        aria-label="New tab"
        title="New tab (Ctrl+T)"
      >
        <PlusIcon />
      </button>

      <button
        type="button"
        className="privacy-button"
        onClick={onOpenDashboard}
        aria-label={`Open privacy dashboard. ${blockedTotal} items blocked this session.`}
        title="Privacy dashboard (Ctrl+Shift+P)"
      >
        <ShieldIcon />
        <span className="privacy-count">{formatBadgeCount(blockedTotal)}</span>
      </button>
    </div>
  );
}
