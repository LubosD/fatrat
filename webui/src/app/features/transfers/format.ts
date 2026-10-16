const UNITS = ['B', 'KB', 'MB', 'GB', 'TB'] as const;

export function formatSize(bytes: number): string {
  if (bytes <= 0) return '0 B';

  let value = bytes;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < UNITS.length - 1) {
    value /= 1024;
    unitIndex++;
  }

  const formatted = unitIndex === 0 ? String(Math.round(value)) : value.toFixed(1);
  return `${formatted} ${UNITS[unitIndex]}`;
}

export function formatSpeed(bytesPerSecond: number): string {
  return `${formatSize(bytesPerSecond)}/s`;
}

export function formatEta(total: number, done: number, bytesPerSecond: number): string {
  const remaining = total - done;
  if (bytesPerSecond <= 0 || remaining <= 0) return '—';

  const totalSeconds = Math.ceil(remaining / bytesPerSecond);

  if (totalSeconds < 60) {
    return `${totalSeconds}s`;
  }
  if (totalSeconds < 3600) {
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${minutes}m ${seconds}s`;
  }
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  return `${hours}h ${minutes}m`;
}

/** Keeps both ends of an over-long name readable: "very-long-na…-name.iso". */
export function elideMiddle(text: string, maxLength: number): string {
  if (maxLength <= 0 || text.length <= maxLength) return text;

  const ellipsis = '...';
  if (maxLength <= ellipsis.length) return ellipsis.slice(0, maxLength);

  const keep = maxLength - ellipsis.length;
  const head = Math.ceil(keep / 2);
  const tail = keep - head;
  return `${text.slice(0, head)}${ellipsis}${tail > 0 ? text.slice(text.length - tail) : ''}`;
}
