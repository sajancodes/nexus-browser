import type { BookmarkItem } from '@nexus/ipc-contracts';
import { initialOfUrl } from '../lib/format';
import { CloseIcon, GlobeIcon, StarIcon } from '../components/icons';

interface BookmarksBarProps {
  bookmarks: BookmarkItem[];
  onNavigate: (url: string) => void;
  onRemoveBookmark: (url: string) => void;
  onOpenManager: () => void;
}

export function BookmarksBar({
  bookmarks,
  onNavigate,
  onRemoveBookmark,
  onOpenManager,
}: BookmarksBarProps) {
  if (bookmarks.length === 0) {
    return (
      <div className="bookmarks-bar empty">
        <span className="bookmarks-empty-tip">
          <StarIcon className="tip-star" /> Star a page or press{' '}
          <kbd>Ctrl+D</kbd> to save bookmarks locally.
        </span>
      </div>
    );
  }

  return (
    <div className="bookmarks-bar" role="toolbar" aria-label="Bookmarks bar">
      <div className="bookmarks-list">
        {bookmarks.slice(0, 16).map((bm) => (
          <div
            key={bm.id}
            className="bookmark-chip"
            onClick={() => onNavigate(bm.url)}
            title={`${bm.title}\n${bm.url}`}
          >
            <span className="bookmark-icon">
              {initialOfUrl(bm.url) || <GlobeIcon />}
            </span>
            <span className="bookmark-title">{bm.title || bm.url}</span>
            <button
              type="button"
              className="bookmark-remove"
              title="Remove bookmark"
              onClick={(e) => {
                e.stopPropagation();
                onRemoveBookmark(bm.url);
              }}
            >
              <CloseIcon />
            </button>
          </div>
        ))}
      </div>
      {bookmarks.length > 16 && (
        <button
          type="button"
          className="bookmarks-more"
          onClick={onOpenManager}
          title="View all bookmarks"
        >
          +{bookmarks.length - 16} more
        </button>
      )}
    </div>
  );
}
