import { useState } from 'react';
import type { MediaItem } from '@nexus/ipc-contracts';
import { CheckIcon, CloseIcon, DownloadIcon, FilmIcon, TrashIcon } from '../../components/icons';

interface MediaPanelProps {
  mediaItems: MediaItem[];
  onDownload: (url: string, title?: string) => Promise<boolean>;
  onClear: () => void;
  onClose: () => void;
}

function formatBytes(bytes: number): string {
  if (bytes <= 0) return 'Stream / Dynamic';
  const units = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return `${(bytes / Math.pow(1024, i)).toFixed(1)} ${units[i]}`;
}

export function MediaPanel({
  mediaItems,
  onDownload,
  onClear,
  onClose,
}: MediaPanelProps) {
  const [downloadingUrl, setDownloadingUrl] = useState<string | null>(null);
  const [copiedUrl, setCopiedUrl] = useState<string | null>(null);

  const handleDownload = async (item: MediaItem) => {
    setDownloadingUrl(item.url);
    try {
      await onDownload(item.url, item.title);
    } finally {
      setTimeout(() => setDownloadingUrl(null), 1500);
    }
  };

  const handleCopy = (url: string) => {
    navigator.clipboard.writeText(url).then(() => {
      setCopiedUrl(url);
      setTimeout(() => setCopiedUrl(null), 2000);
    });
  };

  return (
    <div className="drawer-panel" role="dialog" aria-label="Media Intelligence Sniffer">
      <div className="drawer-header">
        <div className="drawer-title">
          <FilmIcon />
          <span>Media Sniffer ({mediaItems.length})</span>
        </div>
        <div className="drawer-actions">
          {mediaItems.length > 0 && (
            <button
              type="button"
              className="icon-button"
              onClick={onClear}
              title="Clear media history"
              aria-label="Clear all"
            >
              <TrashIcon />
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

      <div className="drawer-content">
        {mediaItems.length === 0 ? (
          <div className="empty-state">
            <FilmIcon className="empty-icon" />
            <p>No media streams detected yet.</p>
            <span className="empty-hint">
              When a webpage loads video, audio, or HLS/DASH streams, NEXUS automatically captures
              the stream endpoints for local download.
            </span>
          </div>
        ) : (
          <div className="media-items">
            {mediaItems.map((item) => (
              <div key={item.id || item.url} className="media-item">
                <div className="media-item-header">
                  <span className={`media-type-badge type-${item.mediaType}`}>
                    {item.mediaType.toUpperCase()}
                  </span>
                  <span className="media-size">{formatBytes(item.sizeBytes)}</span>
                </div>

                <div className="media-title" title={item.title || item.url}>
                  {item.title || item.url}
                </div>

                <div className="media-source" title={item.sourceUrl}>
                  Source: {new URL(item.sourceUrl || 'http://localhost').hostname}
                </div>

                <div className="media-actions">
                  <button
                    type="button"
                    className="action-btn action-primary"
                    onClick={() => handleDownload(item)}
                    disabled={downloadingUrl === item.url}
                  >
                    <DownloadIcon />
                    <span>{downloadingUrl === item.url ? 'Queuing…' : 'Download'}</span>
                  </button>

                  <button
                    type="button"
                    className="action-btn"
                    onClick={() => handleCopy(item.url)}
                    title="Copy stream URL"
                  >
                    {copiedUrl === item.url ? <CheckIcon /> : null}
                    <span>{copiedUrl === item.url ? 'Copied!' : 'Copy Link'}</span>
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
