interface IconProps {
  className?: string;
}

const base = {
  viewBox: '0 0 16 16',
  fill: 'none',
  stroke: 'currentColor',
  strokeWidth: 1.5,
  strokeLinecap: 'round' as const,
  strokeLinejoin: 'round' as const,
  'aria-hidden': true,
  focusable: false,
};

export function ShieldIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M8 1.8 13 3.6v4.2c0 3-2 5.2-5 6.4-3-1.2-5-3.4-5-6.4V3.6L8 1.8Z" />
    </svg>
  );
}

export function LockIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <rect x="3.2" y="7" width="9.6" height="6.3" rx="1.2" />
      <path d="M5.4 7V5.2a2.6 2.6 0 0 1 5.2 0V7" />
    </svg>
  );
}

export function GlobeIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <circle cx="8" cy="8" r="5.7" />
      <path d="M2.3 8h11.4" />
      <path d="M8 2.3c1.7 1.6 2.6 3.5 2.6 5.7S9.7 12.1 8 13.7C6.3 12.1 5.4 10.2 5.4 8S6.3 3.9 8 2.3Z" />
    </svg>
  );
}

export function BackIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M10 3 5 8l5 5" />
    </svg>
  );
}

export function ForwardIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="m6 3 5 5-5 5" />
    </svg>
  );
}

export function ReloadIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M13 8a5 5 0 1 1-1.7-3.8" />
      <path d="M13 2.6v3.6H9.4" />
    </svg>
  );
}

export function StopIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="m4.5 4.5 7 7M11.5 4.5l-7 7" />
    </svg>
  );
}

export function PlusIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M8 3.2v9.6M3.2 8h9.6" />
    </svg>
  );
}

export function CloseIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="m4.6 4.6 6.8 6.8M11.4 4.6l-6.8 6.8" />
    </svg>
  );
}

export function ArrowLeftIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M13 8H3" />
      <path d="M7 4 3 8l4 4" />
    </svg>
  );
}

export function CookieIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M13.5 8a5.5 5.5 0 1 1-5-5.5 2.5 2.5 0 0 0 3.2 3.1 2.5 2.5 0 0 0 1.8 2.4Z" />
      <circle cx="6" cy="6.6" r="0.7" fill="currentColor" stroke="none" />
      <circle cx="6.7" cy="10" r="0.7" fill="currentColor" stroke="none" />
      <circle cx="9.6" cy="9" r="0.6" fill="currentColor" stroke="none" />
    </svg>
  );
}

export function DownloadIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M8 2.5v7.2" />
      <path d="m5.3 7 2.7 2.7L10.7 7" />
      <path d="M3 11.4v1.3c0 .5.4.9.9.9h8.2c.5 0 .9-.4.9-.9v-1.3" />
    </svg>
  );
}

export function LinkIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M6.4 9.6 9.6 6.4" />
      <path d="M7.1 10.8 5.8 12a2.7 2.7 0 0 1-3.8-3.8l1.3-1.3" />
      <path d="M8.9 5.2 10.2 4a2.7 2.7 0 0 1 3.8 3.8l-1.3 1.3" />
    </svg>
  );
}

export function WindowIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <rect x="2.2" y="3" width="11.6" height="10" rx="1.2" />
      <path d="M2.2 6h11.6" />
      <path d="M4.4 4.5h.01M6.2 4.5h.01" />
    </svg>
  );
}

export function CompassIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <circle cx="8" cy="8" r="5.7" />
      <path d="m10.4 5.6-1.5 3.3-3.3 1.5 1.5-3.3 3.3-1.5Z" />
    </svg>
  );
}

export function TrashIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M3 4.5h10" />
      <path d="M6.2 4.5V3.2c0-.4.3-.7.7-.7h2.2c.4 0 .7.3.7.7v1.3" />
      <path d="M4.3 4.5 5 12.8c0 .5.4.9.9.9h4.2c.5 0 .9-.4.9-.9l.7-8.3" />
      <path d="M6.7 7v4M9.3 7v4" />
    </svg>
  );
}

export function CheckIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="m3.3 8.4 3 3 6.4-6.8" />
    </svg>
  );
}

export function SearchIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <circle cx="7" cy="7" r="4.3" />
      <path d="m10.2 10.2 3 3" />
    </svg>
  );
}

export function StarIcon({ className, filled }: IconProps & { filled?: boolean }) {
  return (
    <svg
      {...base}
      className={className ?? 'icon'}
      fill={filled ? 'currentColor' : 'none'}
    >
      <polygon points="8,1.5 10,5.8 14.5,6.3 11,9.6 12,14.2 8,11.8 4,14.2 5,9.6 1.5,6.3 6,5.8" />
    </svg>
  );
}

export function ClockIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <circle cx="8" cy="8" r="5.7" />
      <polyline points="8 4.5 8 8 10.5 9.5" />
    </svg>
  );
}

export function FilmIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <rect x="2" y="3" width="12" height="10" rx="1.5" />
      <line x1="2" y1="6.5" x2="14" y2="6.5" />
      <line x1="2" y1="9.5" x2="14" y2="9.5" />
      <line x1="5.5" y1="3" x2="5.5" y2="6.5" />
      <line x1="10.5" y1="3" x2="10.5" y2="6.5" />
      <line x1="5.5" y1="9.5" x2="5.5" y2="13" />
      <line x1="10.5" y1="9.5" x2="10.5" y2="13" />
    </svg>
  );
}

export function VpnIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <rect x="2" y="5.5" width="12" height="7.5" rx="1.8" />
      <circle cx="8" cy="9.2" r="1.2" />
      <path d="M5.5 5.5V3.8a2.5 2.5 0 0 1 5 0v1.7" />
    </svg>
  );
}

export function GearIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <circle cx="8" cy="8" r="2.5" />
      <path d="M8 1.5v1.8M8 12.7v1.8M1.5 8h1.8M12.7 8h1.8M3.4 3.4l1.3 1.3M11.3 11.3l1.3 1.3M3.4 12.6l1.3-1.3M11.3 4.7l1.3-1.3" />
    </svg>
  );
}

export function FolderIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className ?? 'icon'}>
      <path d="M2 4.2C2 3.5 2.5 3 3.2 3h3.1c.4 0 .7.2.9.5l.8 1.2c.2.3.5.5.9.5h3.9c.7 0 1.2.5 1.2 1.2v6.4c0 .7-.5 1.2-1.2 1.2H3.2C2.5 14 2 13.5 2 12.8V4.2Z" />
    </svg>
  );
}

