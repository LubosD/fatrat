import { ScheduledAction } from '../../core/api/models';

export const WEEKDAYS = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];

/** "Every day", "Mon–Fri", "Mon, Wed, Sun": runs of three or more consecutive days are collapsed. */
export function daysLabel(days: boolean[]): string {
  if (days.length === 7 && days.every(Boolean)) return 'Every day';

  const parts: string[] = [];
  let i = 0;
  while (i < days.length) {
    if (!days[i]) {
      i++;
      continue;
    }
    let end = i;
    while (end + 1 < days.length && days[end + 1]) end++;
    if (end - i >= 2) parts.push(`${WEEKDAYS[i]}–${WEEKDAYS[end]}`);
    else for (let d = i; d <= end; d++) parts.push(WEEKDAYS[d]);
    i = end + 1;
  }
  return parts.join(', ');
}

export function whenLabel(a: ScheduledAction): string {
  if (!a.repeated) return a.at.replace('T', ' ');
  return `${daysLabel(a.days)} ${a.time}`.trim();
}

/** Speed limits are bytes/s on the wire, kB/s on screen; 0 is no limit. */
export function toKiB(bytes: number): number {
  return Math.round(bytes / 1024);
}

export function toBytes(kib: number): number {
  return Math.round(kib) * 1024;
}

function rate(bytes: number): string {
  return bytes === 0 ? 'unlimited' : `${toKiB(bytes)} kB/s`;
}

export function actionLabel(a: ScheduledAction): string {
  switch (a.action) {
    case 'resume':
      return 'Resume all';
    case 'stop':
      return 'Stop all';
    case 'speedLimit':
      return a.speedLimit
        ? `Set speed limit (down ${rate(a.speedLimit.down)}, up ${rate(a.speedLimit.up)})`
        : 'Set speed limit';
  }
}

/** The current local time in the format of <input type="datetime-local">. */
export function nowLocal(): string {
  const d = new Date();
  const p = (n: number) => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}T${p(d.getHours())}:${p(d.getMinutes())}`;
}
