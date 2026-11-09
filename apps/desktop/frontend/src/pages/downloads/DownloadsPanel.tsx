import type { DownloadItem } from '@nexus/ipc-contracts';
import { CloseIcon, DownloadIcon, FolderIcon } from '../../components/icons';

interface DownloadsPanelProps {
  downloads: DownloadItem[];
  onCancel: (guid: string) => void;
  onOpenFile: (guid: string) => void;
  onOpenFolder: (guid: string) => void;
  onClose: () => void;
}

function formatBytes(bytes: number): string {
  if (bytes <= 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return `${(bytes / Math.pow(1024, i)).toFixed(i > 0 ? 1 : 0)} ${units[i]}`;
}

function formatSpeed(bytesPerSec: number): string {
  if (bytesPerSec <= 0) return '';
  return `${formatBytes(bytesPerSec)}/s`;
}

export function DownloadsPanel({
  downloads,
  onCancel,
  onOpenFile,
  onOpenFolder,
  onClose,
}: DownloadsPanelProps) {
  return (
    <div className="drawer-panel" role="dialog" aria-label="Downloads Manager">
      <div className="drawer-header">
        <div className="drawer-title">
          <DownloadIcon />
          <span>Downloads ({downloads.length})</span>
        </div>
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

      <div className="drawer-content">
        {downloads.length === 0 ? (
          <div className="empty-state">
            <DownloadIcon className="empty-icon" />
            <p>No downloads yet.</p>
            <span className="empty-hint">
              Downloaded files will be verified and stored in your local Downloads folder.
            </span>
          </div>
        ) : (
          <div className="download-items">
            {downloads.map((item) => {
              const isProgress = item.state === 'in_progress';
              const isCompleted = item.state === 'completed';
              const isFailed = item.state === 'failed';
              const isCancelled = item.state === 'cancelled';

              return (
                <div key={item.guid || item.id} className="download-item" data-state={item.state}>
                  <div className="download-item-header">
                    <span className="download-filename" title={item.fileName}>
                      {item.fileName}
                    </span>
                    <span className={`download-badge badge-${item.state}`}>
                      {item.state.replace('_', ' ')}
                    </span>
                  </div>

                  {isProgress && (
                    <div className="download-progress-bar">
                      <div
                        className="download-progress-fill"
                        style={{ width: `${Math.max(0, Math.min(100, item.percent))}%` }}
                      />
                    </div>
                  )}

                  <div className="download-details">
                    <span className="download-size">
                      {formatBytes(item.receivedBytes)}
                      {item.totalBytes > 0 ? ` / ${formatBytes(item.totalBytes)}` : ''}
                      {item.percent > 0 ? ` (${item.percent}%)` : ''}
                    </span>
                    {isProgress && item.speed > 0 && (
                      <span className="download-speed">{formatSpeed(item.speed)}</span>
                    )}
                  </div>

                  <div className="download-actions">
                    {isCompleted && (
                      <>
                        <button
                          type="button"
                          className="action-btn"
                          onClick={() => onOpenFile(item.guid)}
                          title="Open File"
                        >
                          Open File
                        </button>
                        <button
                          type="button"
                          className="action-btn"
                          onClick={() => onOpenFolder(item.guid)}
                          title="Show in Folder"
                        >
                          <FolderIcon />
                          <span>Show in Folder</span>
                        </button>
                      </>
                    )}
                    {isProgress && (
                      <button
                        type="button"
                        className="action-btn action-danger"
                        onClick={() => onCancel(item.guid)}
                        title="Cancel Download"
                      >
                        Cancel
                      </button>
                    )}
                    {(isFailed || isCancelled) && (
                      <span className="download-status-note">
                        {isFailed ? 'Download failed' : 'Cancelled by user'}
                      </span>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
}
