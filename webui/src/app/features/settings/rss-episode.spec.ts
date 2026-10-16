import { defaultRange, isValidEpisode } from './rss-episode';

describe('rss-episode', () => {
  it('validates a range bound strictly and a downloaded entry with its flags', () => {
    expect(isValidEpisode('season', 'S01E02', false)).toBe(true);
    expect(isValidEpisode('season', 'S01E02|repack', false)).toBe(false);
    expect(isValidEpisode('season', 'S01E02|repack|proper', true)).toBe(true);
    expect(isValidEpisode('season', 'S1E2', true)).toBe(false);
    expect(isValidEpisode('episode', '0123', false)).toBe(true);
    expect(isValidEpisode('episode', '123', false)).toBe(false);
    expect(isValidEpisode('date', '2026-10-16', false)).toBe(true);
    expect(isValidEpisode('date', '2026-10-16|bogus', true)).toBe(false);
  });

  it('gives the widest range of each mode and none for no tracking', () => {
    expect(defaultRange('season')).toEqual({ from: 'S00E00', to: 'S99E99' });
    expect(defaultRange('episode')).toEqual({ from: '0000', to: '9999' });
    expect(defaultRange('date')).toEqual({ from: '0000-00-00', to: '9999-99-99' });
    expect(defaultRange('none')).toEqual({ from: '', to: '' });
  });
});
