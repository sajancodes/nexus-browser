import type { BrowserTab } from '@nexus/ipc-contracts';

export function formatCount(value: number): string {
  if (!Number.isFinite(value) || value <= 0) {
    return '0';
  }
  return Math.floor(value).toLocaleString('en-US');
}

export function formatBadgeCount(value: number): string {
  if (!Number.isFinite(value) || value <= 0) {
    return '0';
  }
  if (value > 999) {
    return '999+';
  }
  return String(Math.floor(value));
}

export function hostOfUrl(url: string): string {
  try {
    return new URL(url).hostname.replace(/^www\./i, '');
  } catch {
    return '';
  }
}

export function initialOfUrl(url: string): string {
  const host = hostOfUrl(url);
  if (!host) {
    return 'N';
  }
  return host.charAt(0).toUpperCase();
}

export function displayTitleOfTab(tab: BrowserTab): string {
  const title = tab.title.trim();
  if (title) {
    return title;
  }
  if (tab.internal) {
    return 'New Tab';
  }
  const host = hostOfUrl(tab.url);
  return host || tab.url || 'Untitled';
}

export function runQuietly(task: Promise<unknown>, label: string): void {
  void task.catch((error: unknown) => {
    console.error(`[NEXUS] ${label} failed.`, error);
  });
}
