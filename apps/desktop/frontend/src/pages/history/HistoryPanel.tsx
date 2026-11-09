import { useEffect, useState } from 'react';
import type { HistoryItem } from '@nexus/ipc-contracts';
import { initialOfUrl } from '../../lib/format';
import {
  ClockIcon,
  CloseIcon,
  GlobeIcon,
  SearchIcon,
  TrashIcon,
} from '../../components/icons';

interface HistoryPanelProps {
  onNavigate: (url: string) => void;
  onGetHistory: (limit?: number, query?: string) => Promise<HistoryItem[]>;
  onDeleteItem: (id: number) => Promise<void>;
  onClearHistory: (olderThanMs?: number) => Promise<void>;
  onClose: () => void;
}

function formatTimestamp(epochMs: number): string {
  const date = new Date(epochMs);
  const now = new Date();
  const isToday = date.toDateString() === now.toDateString();
  const timeStr = date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  if (isToday) {
    return timeStr;
  }
  return `${date.toLocaleDateString([], { month: 'short', day: 'numeric' })} ${timeStr}`;
}

export function HistoryPanel({
  onNavigate,
  onGetHistory,
  onDeleteItem,
  onClearHistory,
  onClose,
}: HistoryPanelProps) {
  const [items, setItems] = useState<HistoryItem[]>([]);
  const [search, setSearch] = useState('');

  const reload = (query = '') => {
    onGetHistory(100, query).then(setItems);
  };

  useEffect(() => {
    reload(search);
  }, [search]);

  const handleDelete = async (id: number) => {
    await onDeleteItem(id);
    reload(search);
  };

  const handleClearAll = async () => {
    if (window.confirm('Are you sure you want to clear your local browsing history?')) {
      await onClearHistory(0);
      reload('');
    }
  };

  return (
    <div className="drawer-panel" role="dialog" aria-label="Browsing History">
      <div className="drawer-header">
        <div className="drawer-title">
          <ClockIcon />
          <span>Local History ({items.length})</span>
        </div>
        <div className="drawer-actions">
          {items.length > 0 && (
            <button
              type="button"
              className="action-btn action-danger"
              onClick={handleClearAll}
              title="Clear all history"
            >
              <TrashIcon />
              <span>Clear History</span>
            </button>
          )}
          <button
            type="button"
            className="icon-button"
            onClick={onClose}
            aria-label="Close"
            title="Close"
          >
            <CloseIcon />
          </button>
        </div>
      </div>

      <div className="search-bar-inline">
        <SearchIcon />
        <input
          type="text"
          placeholder="Search browsing history…"
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          className="search-input-inline"
        />
        {search && (
          <button
            type="button"
            className="icon-button"
            onClick={() => setSearch('')}
            title="Clear search"
          >
            <CloseIcon />
          </button>
        )}
      </div>

      <div className="drawer-content">
        {items.length === 0 ? (
          <div className="empty-state">
            <ClockIcon className="empty-icon" />
            <p>{search ? 'No matching history entries.' : 'No browsing history recorded.'}</p>
            <span className="empty-hint">
              Browsing history is stored strictly in your local SQLite database and never leaves
              your computer.
            </span>
          </div>
        ) : (
          <div className="history-items">
            {items.map((item) => (
              <div
                key={item.id}
                className="history-item"
                onClick={() => onNavigate(item.url)}
              >
                <span className="history-icon">
                  {initialOfUrl(item.url) || <GlobeIcon />}
                </span>
                <div className="history-text">
                  <span className="history-title" title={item.title || item.url}>
                    {item.title || item.url}
                  </span>
                  <span className="history-url" title={item.url}>
                    {item.url}
                  </span>
                </div>
                <span className="history-time">{formatTimestamp(item.visitTime)}</span>
                <button
                  type="button"
                  className="icon-button history-del-btn"
                  onClick={(e) => {
                    e.stopPropagation();
                    handleDelete(item.id);
                  }}
                  title="Delete from history"
                >
                  <CloseIcon />
                </button>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
