import { RssTvsMode } from '../../core/api/models';

export type RssEpisodeMode = Exclude<RssTvsMode, 'none'>;

/** What an episode id looks like in each tracking mode (the server applies the same rules). */
export const EPISODE_EXAMPLE: Record<RssEpisodeMode, string> = {
  season: 'S01E02',
  episode: '0123',
  date: '2026-10-16',
};

const PATTERNS: Record<RssEpisodeMode, string> = {
  season: 'S\\d{2,}E\\d{2,}',
  episode: '\\d{4,}',
  date: '\\d{4}-\\d{2}-\\d{2}',
};

/** `withFlags`: allow the |trailer, |repack and |proper suffixes of an entry in the downloaded list. */
export function isValidEpisode(mode: RssEpisodeMode, value: string, withFlags: boolean): boolean {
  const suffix = withFlags ? '(\\|(trailer|repack|proper))*' : '';
  return new RegExp(`^${PATTERNS[mode]}${suffix}$`).test(value);
}

export function defaultRange(mode: RssTvsMode): { from: string; to: string } {
  switch (mode) {
    case 'season':
      return { from: 'S00E00', to: 'S99E99' };
    case 'episode':
      return { from: '0000', to: '9999' };
    case 'date':
      return { from: '0000-00-00', to: '9999-99-99' };
    default:
      return { from: '', to: '' };
  }
}
