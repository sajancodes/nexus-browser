import { useEffect, useState } from 'react';
import type { AppInfo, CookieMode, PrivacyState, SitePermissionItem } from '@nexus/ipc-contracts';
import {
  CloseIcon,
  CookieIcon,
  GearIcon,
  ShieldIcon,
  TrashIcon,
} from '../../components/icons';

interface SettingsPanelProps {
  appInfo: AppInfo | null;
  privacy: PrivacyState;
  onSetCookieMode: (mode: CookieMode) => Promise<void>;
  onClearCookies: () => Promise<void>;
  onGetPermissions: () => Promise<SitePermissionItem[]>;
  onClearPermissions: (origin?: string) => Promise<void>;
  onGetSetting: (key: string, defaultValue?: string) => Promise<string>;
  onSetSetting: (key: string, value: string) => Promise<void>;
  onClose: () => void;
}

export function SettingsPanel({
  appInfo,
  privacy,
  onSetCookieMode,
  onClearCookies,
  onGetPermissions,
  onClearPermissions,
  onGetSetting,
  onSetSetting,
  onClose,
}: SettingsPanelProps) {
  const [searchEngine, setSearchEngine] = useState('duckduckgo');
  const [permissions, setPermissions] = useState<SitePermissionItem[]>([]);
  const [cookiesCleared, setCookiesCleared] = useState(false);

  useEffect(() => {
    onGetSetting('search_engine', 'duckduckgo').then(setSearchEngine);
    onGetPermissions().then(setPermissions);
  }, [onGetSetting, onGetPermissions]);

  const handleSearchEngineChange = async (engine: string) => {
    setSearchEngine(engine);
    await onSetSetting('search_engine', engine);
  };

  const handleClearCookies = async () => {
    await onClearCookies();
    setCookiesCleared(true);
    setTimeout(() => setCookiesCleared(false), 2500);
  };

  const handleClearAllPermissions = async () => {
    await onClearPermissions();
    setPermissions([]);
  };

  return (
    <div className="drawer-panel" role="dialog" aria-label="Settings and Privacy Controls">
      <div className="drawer-header">
        <div className="drawer-title">
          <GearIcon />
          <span>Browser Settings & Privacy</span>
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

      <div className="drawer-content settings-content">
        {/* Privacy & Anti-Tracking Mode */}
        <div className="settings-section">
          <h3>
            <ShieldIcon className="section-icon" />
            <span>Anti-Tracking & Cookie Policy</span>
          </h3>

          <div className="cookie-mode-selector">
            <label className={`mode-card ${privacy.cookieMode === 'strict' ? 'active' : ''}`}>
              <input
                type="radio"
                name="cookieMode"
                value="strict"
                checked={privacy.cookieMode === 'strict'}
                onChange={() => onSetCookieMode('strict')}
              />
              <div className="mode-content">
                <strong>Strict Mode (Default)</strong>
                <p>
                  Zero persistent cookies. First-party cookies are isolated and discarded when the
                  tab or browser closes. All third-party cookies and tracking storage are blocked.
                </p>
              </div>
            </label>

            <label className={`mode-card ${privacy.cookieMode === 'balanced' ? 'active' : ''}`}>
              <input
                type="radio"
                name="cookieMode"
                value="balanced"
                checked={privacy.cookieMode === 'balanced'}
                onChange={() => onSetCookieMode('balanced')}
              />
              <div className="mode-content">
                <strong>Balanced Mode</strong>
                <p>
                  Blocks all third-party tracking cookies while allowing persistent first-party
                  cookies for websites requiring stay-logged-in functionality.
                </p>
              </div>
            </label>
          </div>

          <div className="settings-actions-row">
            <button
              type="button"
              className="action-btn action-danger"
              onClick={handleClearCookies}
            >
              <CookieIcon />
              <span>{cookiesCleared ? 'Cookies Cleared!' : 'Clear All Cookies Now'}</span>
            </button>
          </div>
        </div>

        {/* Search Engine */}
        <div className="settings-section">
          <h3>Search Engine</h3>
          <p className="section-hint">
            NEXUS never transmits keystrokes as you type. Omnibox searches run directly on privacy-first engines.
          </p>
          <select
            className="settings-select"
            value={searchEngine}
            onChange={(e) => handleSearchEngineChange(e.target.value)}
          >
            <option value="duckduckgo">DuckDuckGo (Privacy-First Default)</option>
            <option value="brave">Brave Search (Independent Index)</option>
            <option value="startpage">Startpage (Private Google Results)</option>
            <option value="searx">SearXNG (Decentralized Metasearch)</option>
          </select>
        </div>

        {/* Site Permissions */}
        <div className="settings-section">
          <div className="section-header">
            <h3>Site Permissions ({permissions.length})</h3>
            {permissions.length > 0 && (
              <button
                type="button"
                className="action-btn action-danger"
                onClick={handleClearAllPermissions}
              >
                <TrashIcon />
                <span>Reset All</span>
              </button>
            )}
          </div>
          {permissions.length === 0 ? (
            <p className="section-hint">No persistent site permissions granted. NEXUS prompts or denies by default.</p>
          ) : (
            <div className="permissions-list">
              {permissions.map((p, idx) => (
                <div key={idx} className="permission-row">
                  <span className="permission-origin">{p.origin}</span>
                  <span className="permission-type">{p.permission}</span>
                  <span className={`permission-state state-${p.state}`}>{p.state}</span>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* System & Architecture Info */}
        <div className="settings-section system-info-section">
          <h3>Local System & Security Profile</h3>
          {appInfo && (
            <dl className="system-info-grid">
              <div>
                <dt>Browser</dt>
                <dd>{appInfo.name} v{appInfo.version}</dd>
              </div>
              <div>
                <dt>Chromium / CEF</dt>
                <dd>v{appInfo.chromiumVersion} (CEF {appInfo.cefVersion})</dd>
              </div>
              <div>
                <dt>Sandbox Broker</dt>
                <dd>{appInfo.sandbox ? 'Active & Enforced' : 'Disabled'}</dd>
              </div>
              <div>
                <dt>Data Storage</dt>
                <dd className="mono-path" title={appInfo.dataDir}>{appInfo.dataDir}</dd>
              </div>
            </dl>
          )}
        </div>
      </div>
    </div>
  );
}
