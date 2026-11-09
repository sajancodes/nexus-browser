import { useEffect, useState, type ReactNode } from 'react';
import type { AppInfo, CookieMode, PrivacyState } from '@nexus/ipc-contracts';
import { describeError } from '../../lib/ipc';
import { formatCount } from '../../lib/format';
import {
  ArrowLeftIcon,
  CheckIcon,
  CompassIcon,
  CookieIcon,
  DownloadIcon,
  LinkIcon,
  ShieldIcon,
  TrashIcon,
  WindowIcon,
} from '../../components/icons';

interface PrivacyDashboardProps {
  appInfo: AppInfo | null;
  privacy: PrivacyState;
  onClose: () => void;
  onSetCookieMode: (mode: CookieMode) => Promise<void>;
  onClearCookies: () => Promise<void>;
}

interface Notice {
  tone: 'neutral' | 'success' | 'error';
  text: string;
}

interface MetricProps {
  icon: ReactNode;
  label: string;
  value: number;
  hint: string;
}

function Metric({ icon, label, value, hint }: MetricProps) {
  return (
    <div className="metric">
      <div className="metric-label">
        {icon}
        <span>{label}</span>
      </div>
      <div className="metric-value">{formatCount(value)}</div>
      <div className="metric-hint">{hint}</div>
    </div>
  );
}

const IMPLEMENTED: Array<{ title: string; body: string }> = [
  {
    title: 'Sandboxed CEF engine.',
    body: 'Renderer, GPU, network and storage processes run inside Chromium\u2019s sandbox. NEXUS refuses to start if the sandbox broker is missing.',
  },
  {
    title: 'Local profile only.',
    body: 'CEF\u2019s cache and profile paths are redirected under the NEXUS data directory instead of the platform default.',
  },
  {
    title: 'Privileged IPC isolation.',
    body: 'Native bindings exist only in the nexus://app shell. Web content cannot reach the IPC surface.',
  },
  {
    title: 'Deny-by-default permissions.',
    body: 'Camera, microphone, geolocation, notifications and every other web permission are denied without a prompt.',
  },
  {
    title: 'Downloads blocked.',
    body: 'Milestone 1 cancels all downloads instead of writing unreviewed files to disk.',
  },
  {
    title: 'Navigation allowlist.',
    body: 'Content may load only http(s) and the local new-tab page. Popups and unsafe schemes are blocked.',
  },
  {
    title: 'Cookie and referrer control.',
    body: 'Strict mode blocks cookies and removes referrers. Balanced mode blocks third-party cookies and reduces cross-site referrers to the origin.',
  },
  {
    title: 'Strict local CSP.',
    body: 'The shell and new-tab pages are served locally with fixed security headers and no remote scripts, styles or fonts.',
  },
];

const PLANNED: string[] = [
  'Tracker and advertisement blocking lists.',
  'Fingerprinting resistance and user-agent normalization.',
  'DNS-over-HTTPS resolver configuration.',
  'WireGuard VPN tunnelling.',
  'Ephemeral private-browsing profiles.',
  'Per-site permission rules and exceptions.',
  'History, bookmarks and settings stored in SQLite.',
  'Download review and malware scanning.',
];

export function PrivacyDashboard({
  appInfo,
  privacy,
  onClose,
  onSetCookieMode,
  onClearCookies,
}: PrivacyDashboardProps) {
  const [pendingMode, setPendingMode] = useState<CookieMode | null>(null);
  const [clearing, setClearing] = useState(false);
  const [notice, setNotice] = useState<Notice | null>(null);

  useEffect(() => {
    const onKeyDown = (event: globalThis.KeyboardEvent) => {
      if (event.key === 'Escape') {
        event.preventDefault();
        onClose();
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [onClose]);

  const chooseMode = async (mode: CookieMode) => {
    if (pendingMode !== null || mode === privacy.cookieMode) {
      return;
    }
    setPendingMode(mode);
    setNotice(null);
    try {
      await onSetCookieMode(mode);
      setNotice({
        tone: 'success',
        text:
          mode === 'strict'
            ? 'Strict cookie policy enabled.'
            : 'Balanced cookie policy enabled.',
      });
    } catch (error) {
      setNotice({ tone: 'error', text: describeError(error) });
    } finally {
      setPendingMode(null);
    }
  };

  const clearCookies = async () => {
    if (clearing) {
      return;
    }
    setClearing(true);
    setNotice(null);
    try {
      await onClearCookies();
      setNotice({
        tone: 'neutral',
        text: 'Cookie deletion requested. Chromium reports the result in the deleted-cookie counter.',
      });
    } catch (error) {
      setNotice({ tone: 'error', text: describeError(error) });
    } finally {
      setClearing(false);
    }
  };

  const cookieHint =
    privacy.cookieMode === 'strict'
      ? 'All cookies are blocked'
      : 'Third-party cookies are blocked';
  const referrerHint =
    privacy.cookieMode === 'strict'
      ? 'All referrers are removed'
      : 'Cross-site referrers reduced to origin';

  return (
    <div
      className="dashboard"
      role="region"
      aria-labelledby="dashboard-title"
    >
      <header className="dashboard-header">
        <div className="dashboard-brand">
          <span className="brand-mark" aria-hidden="true">
            <ShieldIcon />
          </span>
          <div>
            <h1 id="dashboard-title">Privacy dashboard</h1>
            <p>NEXUS · Milestone 1</p>
          </div>
        </div>
        <button
          type="button"
          className="dashboard-close"
          onClick={onClose}
          title="Close dashboard (Esc)"
        >
          <ArrowLeftIcon />
          <span>Back to browsing</span>
          <kbd>Esc</kbd>
        </button>
      </header>

      <div className="dashboard-content">
        <div className="dashboard-intro">
          <div>
            <h2>Privacy activity in this session</h2>
            <p>
              These counters come from the native browser process. They reset
              when NEXUS restarts and are never transmitted anywhere.
            </p>
          </div>
          <span className="session-tag">This session</span>
        </div>

        <div className="metric-grid">
          <Metric
            icon={<CookieIcon />}
            label="Cookies blocked"
            value={privacy.blockedCookies}
            hint={cookieHint}
          />
          <Metric
            icon={<ShieldIcon />}
            label="Permissions denied"
            value={privacy.deniedPermissions}
            hint="Deny-by-default for web APIs"
          />
          <Metric
            icon={<DownloadIcon />}
            label="Downloads blocked"
            value={privacy.blockedDownloads}
            hint="Downloads are disabled in Milestone 1"
          />
          <Metric
            icon={<LinkIcon />}
            label="Referrers stripped"
            value={privacy.strippedReferrers}
            hint={referrerHint}
          />
          <Metric
            icon={<WindowIcon />}
            label="Popups blocked"
            value={privacy.blockedPopups}
            hint="window.open and target=_blank"
          />
          <Metric
            icon={<CompassIcon />}
            label="Navigations blocked"
            value={privacy.blockedNavigations}
            hint="Unsafe schemes and privileged origins"
          />
          <Metric
            icon={<TrashIcon />}
            label="Cookies deleted"
            value={privacy.deletedCookies}
            hint="Removed by you this session"
          />
        </div>

        <section className="section" aria-labelledby="cookie-heading">
          <h2 id="cookie-heading">Cookie policy</h2>
          <div className="panel">
            <div className="cookie-modes">
              <button
                type="button"
                className="mode-option"
                data-selected={
                  privacy.cookieMode === 'strict' ? 'true' : 'false'
                }
                aria-pressed={privacy.cookieMode === 'strict'}
                disabled={pendingMode !== null}
                onClick={() => void chooseMode('strict')}
              >
                <span className="mode-radio" aria-hidden="true" />
                <span>
                  <span className="mode-title">Strict</span>
                  <span className="mode-description">
                    Block all cookies. Sites cannot set or read cookies, so
                    sign-in and some features may not work.
                  </span>
                </span>
              </button>

              <button
                type="button"
                className="mode-option"
                data-selected={
                  privacy.cookieMode === 'balanced' ? 'true' : 'false'
                }
                aria-pressed={privacy.cookieMode === 'balanced'}
                disabled={pendingMode !== null}
                onClick={() => void chooseMode('balanced')}
              >
                <span className="mode-radio" aria-hidden="true" />
                <span>
                  <span className="mode-title">Balanced</span>
                  <span className="mode-description">
                    Block third-party cookies. First-party cookies are allowed
                    so sites can keep a session within their own origin.
                  </span>
                </span>
              </button>
            </div>

            <p className="mode-note">
              The policy is saved to profile/preferences.json inside your local
              NEXUS data directory. In strict mode referrers are removed
              entirely; in balanced mode cross-site referrers are reduced to
              the origin.
            </p>

            <div className="panel-actions">
              <span className="panel-caption">
                {pendingMode === null
                  ? 'Changes apply to subsequent requests.'
                  : 'Applying cookie policy…'}
              </span>
              <button
                type="button"
                className="primary-button"
                disabled={clearing}
                onClick={() => void clearCookies()}
              >
                <TrashIcon />
                <span>{clearing ? 'Requesting…' : 'Clear all cookies'}</span>
              </button>
            </div>

            {notice ? (
              <div className="status-line" data-tone={notice.tone} role="status">
                {notice.text}
              </div>
            ) : null}
          </div>
        </section>

        <section className="section" aria-labelledby="protections-heading">
          <h2 id="protections-heading">Protections in this build</h2>
          <div className="two-column">
            <div className="panel">
              <ul className="feature-list">
                {IMPLEMENTED.map((item) => (
                  <li key={item.title}>
                    <CheckIcon />
                    <span>
                      <strong>{item.title}</strong> {item.body}
                    </span>
                  </li>
                ))}
              </ul>
            </div>

            <div className="panel">
              <h3 className="panel-title">Planned for later milestones</h3>
              <ul className="feature-list planned-list">
                {PLANNED.map((item) => (
                  <li key={item}>
                    <span className="planned-dot" aria-hidden="true" />
                    <span>{item}</span>
                  </li>
                ))}
              </ul>
              <p className="mode-note">
                These items are not implemented in this build. They are listed
                here so this dashboard never implies protection that is not
                active.
              </p>
            </div>
          </div>
        </section>

        <section className="section" aria-labelledby="about-heading">
          <h2 id="about-heading">About this build</h2>
          <div className="panel">
            <dl className="about-grid">
              <div className="about-item">
                <dt>Version</dt>
                <dd>{appInfo ? appInfo.version : '—'}</dd>
              </div>
              <div className="about-item">
                <dt>Milestone</dt>
                <dd>{appInfo ? String(appInfo.milestone) : '—'}</dd>
              </div>
              <div className="about-item">
                <dt>Platform</dt>
                <dd>{appInfo ? appInfo.platform : '—'}</dd>
              </div>
              <div className="about-item">
                <dt>Sandbox</dt>
                <dd>
                  {appInfo
                    ? appInfo.sandbox
                      ? 'Enabled'
                      : 'Not reported'
                    : '—'}
                </dd>
              </div>
              <div className="about-item">
                <dt>CEF</dt>
                <dd className="mono">{appInfo ? appInfo.cefVersion : '—'}</dd>
              </div>
              <div className="about-item">
                <dt>Chromium</dt>
                <dd className="mono">
                  {appInfo ? appInfo.chromiumVersion : '—'}
                </dd>
              </div>
              <div className="about-item about-wide">
                <dt>Local data directory</dt>
                <dd className="mono">{appInfo ? appInfo.dataDir : '—'}</dd>
              </div>
            </dl>
          </div>
        </section>

        <footer className="dashboard-footer">
          <span>This build contains no telemetry.</span>
          <span>
            Profile and cache are redirected to the NEXUS data directory.
          </span>
        </footer>
      </div>
    </div>
  );
}
