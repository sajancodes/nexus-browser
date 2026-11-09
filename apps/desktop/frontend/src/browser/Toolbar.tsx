import { useEffect, useRef, useState, type FormEvent } from 'react';
import type { BrowserTab } from '@nexus/ipc-contracts';
import { describeError } from '../lib/ipc';
import {
  BackIcon,
  ForwardIcon,
  GlobeIcon,
  LockIcon,
  ReloadIcon,
  ShieldIcon,
  StopIcon,
} from '../components/icons';

interface ToolbarProps {
  activeTab: BrowserTab | undefined;
  focusSignal: number;
  onNavigate: (input: string) => Promise<void>;
  onBack: () => void;
  onForward: () => void;
  onReload: () => void;
  onStop: () => void;
}

export function Toolbar({
  activeTab,
  focusSignal,
  onNavigate,
  onBack,
  onForward,
  onReload,
  onStop,
}: ToolbarProps) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState('');
  const [error, setError] = useState<string | null>(null);

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
  }, [activeTab?.id]);

  useEffect(() => {
    if (!error) {
      return;
    }
    const timer = window.setTimeout(() => setError(null), 7000);
    return () => window.clearTimeout(timer);
  }, [error]);

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const input = draft.trim();
    if (!input) {
      inputRef.current?.focus();
      return;
    }

    setError(null);
    try {
      await onNavigate(input);
      setEditing(false);
      inputRef.current?.blur();
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

  return (
    <div className="toolbar">
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
            title="Stop loading"
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
          onBlur={() => setEditing(false)}
          onKeyDown={(event) => {
            if (event.key === 'Escape') {
              event.preventDefault();
              setEditing(false);
              setError(null);
              inputRef.current?.blur();
            }
          }}
          aria-label="Address and search bar"
          placeholder="Search with DuckDuckGo or enter address"
          autoComplete="off"
          spellCheck={false}
        />
        {error ? (
          <span className="address-error" role="alert" title={error}>
            {error}
          </span>
        ) : null}
      </form>
    </div>
  );
}
