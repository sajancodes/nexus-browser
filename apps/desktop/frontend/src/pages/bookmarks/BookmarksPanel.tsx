import { useState } from 'react';
import type { BookmarkItem } from '@nexus/ipc-contracts';
import { initialOfUrl } from '../../lib/format';
import {
  CloseIcon,
  GlobeIcon,
  PlusIcon,
  SearchIcon,
  StarIcon,
} from '../../components/icons';

interface BookmarksPanelProps {
  bookmarks: BookmarkItem[];
  onNavigate: (url: string) => void;
  onRemoveBookmark: (url: string) => void;
  onAddBookmark: (url: string, title: string) => Promise<BookmarkItem>;
  onClose: () => void;
}

export function BookmarksPanel({
  bookmarks,
  onNavigate,
  onRemoveBookmark,
  onAddBookmark,
  onClose,
}: BookmarksPanelProps) {
  const [search, setSearch] = useState('');
  const [isAdding, setIsAdding] = useState(false);
  const [newTitle, setNewTitle] = useState('');
  const [newUrl, setNewUrl] = useState('');

  const filtered = bookmarks.filter(
    (b) =>
      b.title.toLowerCase().includes(search.toLowerCase()) ||
      b.url.toLowerCase().includes(search.toLowerCase()),
  );

  const handleAdd = async () => {
    if (!newUrl.trim()) return;
    await onAddBookmark(newUrl.trim(), newTitle.trim() || newUrl.trim());
    setNewTitle('');
    setNewUrl('');
    setIsAdding(false);
  };

  return (
    <div className="drawer-panel" role="dialog" aria-label="Bookmarks Manager">
      <div className="drawer-header">
        <div className="drawer-title">
          <StarIcon filled className="drawer-star-icon" />
          <span>Bookmarks ({bookmarks.length})</span>
        </div>
        <div className="drawer-actions">
          <button
            type="button"
            className="action-btn"
            onClick={() => setIsAdding(!isAdding)}
          >
            <PlusIcon />
            <span>{isAdding ? 'Cancel' : 'Add'}</span>
          </button>
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

      {isAdding && (
        <div className="add-bookmark-form">
          <input
            type="text"
            placeholder="Page Title"
            value={newTitle}
            onChange={(e) => setNewTitle(e.target.value)}
            className="input-field"
          />
          <input
            type="text"
            placeholder="URL (https://…)"
            value={newUrl}
            onChange={(e) => setNewUrl(e.target.value)}
            className="input-field"
          />
          <button
            type="button"
            className="primary-button"
            onClick={handleAdd}
            disabled={!newUrl.trim()}
          >
            Save Bookmark
          </button>
        </div>
      )}

      <div className="search-bar-inline">
        <SearchIcon />
        <input
          type="text"
          placeholder="Search bookmarks…"
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
        {filtered.length === 0 ? (
          <div className="empty-state">
            <StarIcon className="empty-icon" />
            <p>{search ? 'No bookmarks match your search.' : 'No bookmarks saved yet.'}</p>
            <span className="empty-hint">
              Click the star icon in the address bar on any page to bookmark it locally.
            </span>
          </div>
        ) : (
          <div className="bookmark-items">
            {filtered.map((item) => (
              <div
                key={item.id}
                className="bookmark-item"
                onClick={() => onNavigate(item.url)}
              >
                <span className="bookmark-icon">
                  {initialOfUrl(item.url) || <GlobeIcon />}
                </span>
                <div className="bookmark-text">
                  <span className="bookmark-title" title={item.title}>
                    {item.title || item.url}
                  </span>
                  <span className="bookmark-url" title={item.url}>
                    {item.url}
                  </span>
                </div>
                <button
                  type="button"
                  className="icon-button bookmark-del-btn"
                  onClick={(e) => {
                    e.stopPropagation();
                    onRemoveBookmark(item.url);
                  }}
                  title="Remove bookmark"
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
