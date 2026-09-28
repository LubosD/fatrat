import { formatSize, formatSpeed, formatEta } from './format';

describe('formatSize', () => {
  it('formats bytes below 1 KB as B', () => {
    expect(formatSize(512)).toBe('512 B');
  });

  it('formats KB with one decimal', () => {
    expect(formatSize(1536)).toBe('1.5 KB');
  });

  it('formats MB with one decimal', () => {
    expect(formatSize(5 * 1024 * 1024)).toBe('5.0 MB');
  });

  it('formats GB with one decimal', () => {
    expect(formatSize(2.5 * 1024 * 1024 * 1024)).toBe('2.5 GB');
  });

  it('formats zero as 0 B', () => {
    expect(formatSize(0)).toBe('0 B');
  });
});

describe('formatSpeed', () => {
  it('appends "/s" to a formatted size', () => {
    expect(formatSpeed(2048)).toBe('2.0 KB/s');
  });

  it('formats zero speed as 0 B/s', () => {
    expect(formatSpeed(0)).toBe('0 B/s');
  });
});

describe('formatEta', () => {
  it('returns an em dash when speed is zero (unknown ETA)', () => {
    expect(formatEta(1000, 500, 0)).toBe('—');
  });

  it('returns an em dash when the transfer is already complete', () => {
    expect(formatEta(1000, 1000, 500)).toBe('—');
  });

  it('formats seconds remaining under a minute as "Ns"', () => {
    expect(formatEta(1000, 500, 500)).toBe('1s');
  });

  it('formats minutes and seconds under an hour as "Mm Ss"', () => {
    expect(formatEta(betterTotal(150), 0, 1)).toBe('2m 30s');
  });

  it('formats hours and minutes at or above an hour as "Hh Mm"', () => {
    expect(formatEta(betterTotal(3720), 0, 1)).toBe('1h 2m');
  });

  function betterTotal(seconds: number): number {
    // total - done == seconds remaining, at a fixed speed of 1 B/s, keeps the math in the test obvious.
    return seconds;
  }
});
