import { useEffect, useRef, useState, type FormEvent } from 'react';
import type { BrowserTab, HistoryItem } from '@nexus/ipc-contracts';
import { describeError } from '../lib/ipc';
import { initialOfUrl } from '../lib/format';
import {
  BackIcon,
  ClockIcon,
  DownloadIcon,
  FilmIcon,
  ForwardIcon,
  GearIcon,
  GlobeIcon,
  LockIcon,
  ReloadIcon,
  SearchIcon,
  ShieldIcon,
  StarIcon,
  StopIcon,
  VpnIcon,
} from '../components/icons';

interface ToolbarProps {
  activeTab: BrowserTab | undefined;
  focusSignal: number;
  isBookmarked: boolean;
  zoomLevel: number;
  mediaCount: number;
  downloadsCount: number;
  activeDownloadsCount: number;
  vpnConnected: boolean;
  activePanel: string;
  onNavigate: (input: string) => Promise<void>;
  onBack: () => void;
  onForward: () => void;
  onReload: () => void;
  onStop: () => void;
  onToggleBookmark: () => void;
  onAutocomplete: (prefix: string) => Promise<HistoryItem[]>;
  onZoomIn: () => void;
  onZoomOut: () => void;
  onResetZoom: () => void;
  onToggleFind: () => void;
  onTogglePanel: (
    panel:
      | 'none'
      | 'privacy'
      | 'history'
      | 'bookmarks'
      | 'downloads'
      | 'media'
      | 'vpn'
      | 'settings',
  ) => void;
}

export function Toolbar({
  activeTab,
  focusSignal,
  isBookmarked,
  zoomLevel,
  mediaCount,
  downloadsCount,
  activeDownloadsCount,
  vpnConnected,
  activePanel,
  onNavigate,
  onBack,
  onForward,
  onReload,
  onStop,
  onToggleBookmark,
  onAutocomplete,
  onZoomIn,
  onZoomOut,
  onResetZoom,
  onToggleFind,
  onTogglePanel,
}: ToolbarProps) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [suggestions, setSuggestions] = useState<HistoryItem[]>([]);
  const [showSuggestions, setShowSuggestions] = useState(false);
  const [selectedIndex, setSelectedIndex] = useState(-1);

  const currentUrl = activeTab && !activeTab.internal ? activeTab.url : '';
  const currentUrlRef = useRef(currentUrl);
  currentUrlRef.current = currentUrl;

  useEffect(() => {
    if (focusSignal === 0) {
      return;
    }
    const input = inputRef.current;
    if (!input) {
      return;
    }
    setEditing(true);
    setDraft(currentUrlRef.current);
    input.focus();
    const frame = window.requestAnimationFrame(() => input.select());
    return () => window.cancelAnimationFrame(frame);
  }, [focusSignal]);

  useEffect(() => {
    setEditing(false);
    setError(null);
    setShowSuggestions(false);
  }, [activeTab?.id]);

  useEffect(() => {
    if (!error) {
      return;
    }
    const timer = window.setTimeout(() => setError(null), 7000);
    return () => window.clearTimeout(timer);
  }, [error]);

  // Autocomplete debouncing
  useEffect(() => {
    if (!editing || draft.trim().length === 0) {
      setSuggestions([]);
      setShowSuggestions(false);
      return;
    }

    const timer = window.setTimeout(async () => {
      try {
        const results = await onAutocomplete(draft.trim());
        setSuggestions(results);
        setShowSuggestions(results.length > 0);
        setSelectedIndex(-1);
      } catch {
        setSuggestions([]);
        setShowSuggestions(false);
      }
    }, 120);

    return () => window.clearTimeout(timer);
  }, [draft, editing, onAutocomplete]);

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    let input = draft.trim();
    if (selectedIndex >= 0 && suggestions[selectedIndex]) {
      input = suggestions[selectedIndex].url;
    }
    if (!input) {
      inputRef.current?.focus();
      return;
    }

    setError(null);
    setShowSuggestions(false);
    try {
      await onNavigate(input);
      setEditing(false);
      inputRef.current?.blur();
    } catch (cause) {
      setError(describeError(cause));
    }
  };

  const handleSuggestionClick = async (url: string) => {
    setShowSuggestions(false);
    setEditing(false);
    try {
      await onNavigate(url);
    } catch (cause) {
      setError(describeError(cause));
    }
  };

  let security: 'internal' | 'secure' | 'insecure' | 'neutral';
  if (activeTab?.internal) {
    security = 'internal';
  } else if (activeTab?.secure) {
    security = 'secure';
  } else if (activeTab) {
    security = 'insecure';
  } else {
    security = 'neutral';
  }

  const securityTitle =
    security === 'internal'
      ? 'Local NEXUS page'
      : security === 'secure'
        ? 'Connection is HTTPS'
        : security === 'insecure'
          ? 'Connection is not HTTPS'
          : 'No page loaded';

  const SecurityIcon =
    security === 'internal'
      ? ShieldIcon
      : security === 'insecure'
        ? GlobeIcon
        : LockIcon;

  const displayZoom = Math.round(Math.pow(1.2, zoomLevel) * 100);

  return (
    <div className="toolbar">
      {/* Navigation history controls */}
      <div className="nav-buttons">
        <button
          type="button"
          className="icon-button"
          onClick={onBack}
          disabled={!activeTab?.canGoBack}
          aria-label="Back"
          title="Back (Alt+Left)"
        >
          <BackIcon />
        </button>
        <button
          type="button"
          className="icon-button"
          onClick={onForward}
          disabled={!activeTab?.canGoForward}
          aria-label="Forward"
          title="Forward (Alt+Right)"
        >
          <ForwardIcon />
        </button>
        {activeTab?.loading ? (
          <button
            type="button"
            className="icon-button"
            onClick={onStop}
            aria-label="Stop loading"
            title="Stop loading (Esc)"
          >
            <StopIcon />
          </button>
        ) : (
          <button
            type="button"
            className="icon-button"
            onClick={onReload}
            disabled={!activeTab}
            aria-label="Reload"
            title="Reload (Ctrl+R)"
          >
            <ReloadIcon />
          </button>
        )}
      </div>

      {/* Omnibox / Address Bar */}
      <div className="address-container">
        <form
          className="address-bar"
          data-secure={security === 'secure' ? 'true' : 'false'}
          data-insecure={security === 'insecure' ? 'true' : 'false'}
          data-invalid={error ? 'true' : 'false'}
          onSubmit={handleSubmit}
          noValidate
        >
          <span className="address-icon" title={securityTitle}>
            <SecurityIcon />
          </span>

          <input
            ref={inputRef}
            className="address-input"
            type="text"
            value={editing ? draft : currentUrl}
            onChange={(event) => {
              setEditing(true);
              setDraft(event.currentTarget.value);
              if (error) {
                setError(null);
              }
            }}
            onFocus={(event) => {
              if (!editing) {
                setDraft(currentUrl);
                setEditing(true);
              }
              event.currentTarget.select();
            }}
            onBlur={() => {
              // Delay hiding suggestions so click event on item can fire
              setTimeout(() => {
                setShowSuggestions(false);
                setEditing(false);
              }, 200);
            }}
            onKeyDown={(event) => {
              if (event.key === 'Escape') {
                event.preventDefault();
                setShowSuggestions(false);
                setEditing(false);
                setError(null);
                inputRef.current?.blur();
              } else if (event.key === 'ArrowDown') {
                if (suggestions.length > 0) {
                  event.preventDefault();
                  setSelectedIndex((prev) =>
                    prev < suggestions.length - 1 ? prev + 1 : 0,
                  );
                }
              } else if (event.key === 'ArrowUp') {
                if (suggestions.length > 0) {
                  event.preventDefault();
                  setSelectedIndex((prev) =>
                    prev > 0 ? prev - 1 : suggestions.length - 1,
                  );
                }
              }
            }}
            aria-label="Address and search bar"
            placeholder="Search with Google or enter address"
            autoComplete="off"
            spellCheck={false}
          />

          {/* Bookmark Star inside Address Bar */}
          {activeTab && !activeTab.internal && (
            <button
              type="button"
              className={`address-action-btn ${isBookmarked ? 'bookmarked' : ''}`}
              onClick={(e) => {
                e.preventDefault();
                onToggleBookmark();
              }}
              title={isBookmarked ? 'Remove Bookmark (Ctrl+D)' : 'Bookmark Page (Ctrl+D)'}
              aria-label="Bookmark"
            >
              <StarIcon filled={isBookmarked} />
            </button>
          )}

          {error ? (
            <span className="address-error" role="alert" title={error}>
              {error}
            </span>
          ) : null}
        </form>

        {/* Omnibox Autocomplete Dropdown */}
        {showSuggestions && suggestions.length > 0 && (
          <div className="autocomplete-dropdown" role="listbox">
            {suggestions.map((item, index) => (
              <div
                key={item.id}
                className={`autocomplete-item ${index === selectedIndex ? 'selected' : ''}`}
                onMouseDown={() => handleSuggestionClick(item.url)}
              >
                <span className="autocomplete-icon">
                  {initialOfUrl(item.url) || <GlobeIcon />}
                </span>
                <div className="autocomplete-info">
                  <span className="autocomplete-title">{item.title || item.url}</span>
                  <span className="autocomplete-url">{item.url}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Action buttons and panels toggle */}
      <div className="toolbar-actions">
        {/* Zoom controls if zoomed */}
        {zoomLevel !== 0 && (
          <div className="zoom-badge-group">
            <button
              type="button"
              className="zoom-btn"
              onClick={onZoomOut}
              title="Zoom out (Ctrl+-)"
            >
              -
            </button>
            <button
              type="button"
              className="zoom-level-text"
              onClick={onResetZoom}
              title="Reset zoom (Ctrl+0)"
            >
              {displayZoom}%
            </button>
            <button
              type="button"
              className="zoom-btn"
              onClick={onZoomIn}
              title="Zoom in (Ctrl++)"
            >
              +
            </button>
          </div>
        )}

        {/* Find in page */}
        <button
          type="button"
          className="icon-button"
          onClick={onToggleFind}
          title="Find in page (Ctrl+F)"
          aria-label="Find in page"
        >
          <SearchIcon />
        </button>

        {/* WireGuard VPN status pill */}
        <button
          type="button"
          className={`toolbar-pill ${vpnConnected ? 'pill-vpn-active' : ''} ${activePanel === 'vpn' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'vpn' ? 'none' : 'vpn')}
          title={vpnConnected ? 'WireGuard VPN: Connected' : 'WireGuard VPN: Disconnected'}
        >
          <VpnIcon />
          <span>{vpnConnected ? 'VPN ON' : 'VPN'}</span>
        </button>

        {/* Media Sniffer button with badge */}
        <button
          type="button"
          className={`icon-button toolbar-badge-btn ${activePanel === 'media' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'media' ? 'none' : 'media')}
          title="Media Sniffer & Stream Downloader"
          aria-label="Media Sniffer"
        >
          <FilmIcon />
          {mediaCount > 0 && <span className="action-badge badge-media">{mediaCount}</span>}
        </button>

        {/* Downloads button with badge */}
        <button
          type="button"
          className={`icon-button toolbar-badge-btn ${activePanel === 'downloads' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'downloads' ? 'none' : 'downloads')}
          title="Downloads Manager"
          aria-label="Downloads"
        >
          <DownloadIcon />
          {activeDownloadsCount > 0 ? (
            <span className="action-badge badge-downloading">{activeDownloadsCount}</span>
          ) : downloadsCount > 0 ? (
            <span className="action-badge">{downloadsCount}</span>
          ) : null}
        </button>

        {/* Bookmarks Manager */}
        <button
          type="button"
          className={`icon-button ${activePanel === 'bookmarks' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'bookmarks' ? 'none' : 'bookmarks')}
          title="Bookmarks (Ctrl+B)"
          aria-label="Bookmarks"
        >
          <StarIcon />
        </button>

        {/* History Manager */}
        <button
          type="button"
          className={`icon-button ${activePanel === 'history' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'history' ? 'none' : 'history')}
          title="History (Ctrl+H)"
          aria-label="History"
        >
          <ClockIcon />
        </button>

        {/* Settings */}
        <button
          type="button"
          className={`icon-button ${activePanel === 'settings' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'settings' ? 'none' : 'settings')}
          title="Browser Settings"
          aria-label="Settings"
        >
          <GearIcon />
        </button>

        {/* Privacy Shield */}
        <button
          type="button"
          className={`icon-button shield-btn ${activePanel === 'privacy' ? 'active' : ''}`}
          onClick={() => onTogglePanel(activePanel === 'privacy' ? 'none' : 'privacy')}
          title="Privacy Shield & Tracker Protection"
          aria-label="Privacy Shield"
        >
          <ShieldIcon />
        </button>
      </div>
    </div>
  );
}
