import { useEffect, useState } from 'react';
import type { VpnConfigItem, VpnTunnelStatus } from '@nexus/ipc-contracts';
import { CheckIcon, CloseIcon, PlusIcon, ShieldIcon, TrashIcon, VpnIcon } from '../../components/icons';

interface VpnPanelProps {
  status: VpnTunnelStatus;
  onConnect: (name: string, content?: string) => Promise<VpnTunnelStatus>;
  onDisconnect: () => Promise<VpnTunnelStatus>;
  onGetConfigs: () => Promise<VpnConfigItem[]>;
  onImportConfig: (name: string, content: string) => Promise<boolean>;
  onDeleteConfig: (id: number) => Promise<void>;
  onSetKillSwitch: (enabled: boolean) => Promise<void>;
  onClose: () => void;
}

function formatBytes(bytes: number): string {
  if (bytes <= 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return `${(bytes / Math.pow(1024, i)).toFixed(1)} ${units[i]}`;
}

function formatUptime(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  return `${h > 0 ? `${h}h ` : ''}${m}m ${s}s`;
}

export function VpnPanel({
  status,
  onConnect,
  onDisconnect,
  onGetConfigs,
  onImportConfig,
  onDeleteConfig,
  onSetKillSwitch,
  onClose,
}: VpnPanelProps) {
  const [configs, setConfigs] = useState<VpnConfigItem[]>([]);
  const [selectedConfig, setSelectedConfig] = useState<string>('');
  const [isImporting, setIsImporting] = useState(false);
  const [newConfigName, setNewConfigName] = useState('');
  const [newConfigContent, setNewConfigContent] = useState('');
  const [importError, setImportError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    onGetConfigs().then((list) => {
      setConfigs(list);
      if (list.length > 0 && !selectedConfig) {
        const active = list.find((c) => c.isActive);
        setSelectedConfig(active ? active.name : list[0].name);
      }
    });
  }, [onGetConfigs, selectedConfig]);

  const handleToggleConnect = async () => {
    setBusy(true);
    try {
      if (status.status === 'connected') {
        await onDisconnect();
      } else {
        await onConnect(selectedConfig);
      }
    } finally {
      setBusy(false);
    }
  };

  const handleSaveImport = async () => {
    if (!newConfigName.trim() || !newConfigContent.trim()) {
      setImportError('Please provide both a profile name and WireGuard .conf content.');
      return;
    }
    setImportError(null);
    setBusy(true);
    try {
      const ok = await onImportConfig(newConfigName.trim(), newConfigContent.trim());
      if (ok) {
        setIsImporting(false);
        setNewConfigName('');
        setNewConfigContent('');
        const updated = await onGetConfigs();
        setConfigs(updated);
        setSelectedConfig(newConfigName.trim());
      } else {
        setImportError('Failed to parse or save WireGuard config file.');
      }
    } catch (e) {
      setImportError(e instanceof Error ? e.message : 'Import failed');
    } finally {
      setBusy(false);
    }
  };

  const handleDelete = async (id: number) => {
    await onDeleteConfig(id);
    const updated = await onGetConfigs();
    setConfigs(updated);
    if (updated.length > 0) {
      setSelectedConfig(updated[0].name);
    } else {
      setSelectedConfig('');
    }
  };

  const isConnected = status.status === 'connected';

  return (
    <div className="drawer-panel" role="dialog" aria-label="WireGuard VPN Tunnel">
      <div className="drawer-header">
        <div className="drawer-title">
          <VpnIcon />
          <span>WireGuard VPN</span>
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
        {/* Connection Master Card */}
        <div className={`vpn-status-card status-${status.status}`}>
          <div className="vpn-status-indicator">
            <span className={`vpn-dot dot-${status.status}`} />
            <span className="vpn-status-text">
              {status.status.toUpperCase()}
            </span>
          </div>

          {isConnected && (
            <div className="vpn-metrics-grid">
              <div className="metric-box">
                <span className="metric-label">Uptime</span>
                <span className="metric-value">{formatUptime(status.uptimeSeconds)}</span>
              </div>
              <div className="metric-box">
                <span className="metric-label">Interface IP</span>
                <span className="metric-value">{status.interfaceIp || '10.0.0.2'}</span>
              </div>
              <div className="metric-box">
                <span className="metric-label">Sent</span>
                <span className="metric-value">{formatBytes(status.bytesSent)}</span>
              </div>
              <div className="metric-box">
                <span className="metric-label">Received</span>
                <span className="metric-value">{formatBytes(status.bytesReceived)}</span>
              </div>
            </div>
          )}

          {status.errorMessage && (
            <div className="vpn-error-banner">{status.errorMessage}</div>
          )}

          <div className="vpn-main-controls">
            <button
              type="button"
              className={`primary-button vpn-toggle-btn ${isConnected ? 'btn-disconnect' : 'btn-connect'}`}
              onClick={handleToggleConnect}
              disabled={busy || (!isConnected && !selectedConfig)}
            >
              <ShieldIcon />
              <span>{isConnected ? 'Disconnect Tunnel' : 'Connect VPN'}</span>
            </button>
          </div>
        </div>

        {/* Security Options: Kill-switch and DNS Leak Protection */}
        <div className="vpn-section">
          <h4>Privacy & Leak Protection</h4>
          <label className="checkbox-row">
            <input
              type="checkbox"
              checked={status.killSwitchEnabled}
              onChange={(e) => onSetKillSwitch(e.target.checked)}
            />
            <div className="checkbox-desc">
              <strong>Network Kill-Switch</strong>
              <span>Block all internet traffic outside the WireGuard tunnel.</span>
            </div>
          </label>

          <label className="checkbox-row">
            <input
              type="checkbox"
              checked={status.dnsLeakProtection}
              disabled
              readOnly
            />
            <div className="checkbox-desc">
              <strong>DNS Leak Protection</strong>
              <span>Force all DNS queries through the tunnel DNS ({status.dns || 'Local Secure'}).</span>
            </div>
          </label>
        </div>

        {/* Configurations list */}
        <div className="vpn-section">
          <div className="section-header">
            <h4>Saved Profiles ({configs.length})</h4>
            <button
              type="button"
              className="action-btn"
              onClick={() => setIsImporting(!isImporting)}
            >
              <PlusIcon />
              <span>{isImporting ? 'Cancel' : 'Import Profile'}</span>
            </button>
          </div>

          {isImporting && (
            <div className="import-box">
              <input
                type="text"
                placeholder="Profile Name (e.g., Zurich-Pro, Home-Server)"
                value={newConfigName}
                onChange={(e) => setNewConfigName(e.target.value)}
                className="input-field"
              />
              <textarea
                placeholder="Paste WireGuard [Interface] and [Peer] configuration here..."
                value={newConfigContent}
                onChange={(e) => setNewConfigContent(e.target.value)}
                rows={6}
                className="input-textarea"
              />
              {importError && <div className="field-error">{importError}</div>}
              <button
                type="button"
                className="primary-button"
                onClick={handleSaveImport}
                disabled={busy}
              >
                <CheckIcon />
                <span>Save Profile</span>
              </button>
            </div>
          )}

          <div className="configs-list">
            {configs.length === 0 && !isImporting && (
              <div className="empty-state-sm">
                No WireGuard profiles saved. Click "Import Profile" to add one.
              </div>
            )}
            {configs.map((cfg) => (
              <div
                key={cfg.id}
                className={`config-item ${selectedConfig === cfg.name ? 'selected' : ''}`}
                onClick={() => setSelectedConfig(cfg.name)}
              >
                <div className="config-info">
                  <span className="config-name">{cfg.name}</span>
                  <span className="config-endpoint">{cfg.endpoint || 'Peer Server'}</span>
                </div>
                <div className="config-actions">
                  <button
                    type="button"
                    className="icon-button"
                    onClick={(e) => {
                      e.stopPropagation();
                      handleDelete(cfg.id);
                    }}
                    title="Delete profile"
                  >
                    <TrashIcon />
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
