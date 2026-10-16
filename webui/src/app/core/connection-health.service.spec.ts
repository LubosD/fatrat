import { Subject, of, throwError } from 'rxjs';
import { ConnectionHealth } from './connection-health.service';

describe('ConnectionHealth', () => {
  it('flags a failing source and clears it on the next success', () => {
    const health = new ConnectionHealth();
    expect(health.refreshFailing()).toBe(false);

    health.track('a', throwError(() => new Error('down'))).subscribe({ error: () => {} });
    expect(health.refreshFailing()).toBe(true);

    health.track('a', of(1)).subscribe();
    expect(health.refreshFailing()).toBe(false);
  });

  it('stays failing until every failing source has recovered', () => {
    const health = new ConnectionHealth();
    health.track('a', throwError(() => new Error())).subscribe({ error: () => {} });
    health.track('b', throwError(() => new Error())).subscribe({ error: () => {} });
    health.track('a', of(1)).subscribe();
    expect(health.refreshFailing()).toBe(true);
    health.track('b', of(1)).subscribe();
    expect(health.refreshFailing()).toBe(false);
  });

  it('passes values and errors through', () => {
    const health = new ConnectionHealth();
    const s = new Subject<number>();
    const seen: number[] = [];
    health.track('a', s).subscribe((v) => seen.push(v));
    s.next(3);
    expect(seen).toEqual([3]);
  });
});
