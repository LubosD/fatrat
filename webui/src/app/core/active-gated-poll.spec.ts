import { BehaviorSubject, Observable, Subject, of } from 'rxjs';
import { activeGatedPoll, PollEmission } from './active-gated-poll';

type Fetch = (selection: string) => Observable<string>;

describe('activeGatedPoll', () => {
  it('does not fetch while inactive', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(false);
    selection$.next('s1');

    expect(fetch).not.toHaveBeenCalled();
    expect(emissions).toEqual([]);
  });

  it('fetches immediately on activation, flagged as an activation emission', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(true);
    selection$.next('s1');

    expect(fetch).toHaveBeenCalledWith('s1');
    expect(emissions).toEqual([{ value: 'data', isActivation: true }]);
  });

  it('refetches on every poll tick, flagged as a non-activation emission', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(true);
    selection$.next('s1');
    ticks$.next(1);

    expect(fetch).toHaveBeenCalledTimes(2);
    expect(emissions[1]).toEqual({ value: 'data', isActivation: false });
  });

  it('stops fetching when deactivated', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe();

    active$.next(true);
    selection$.next('s1');
    fetch.mockClear();

    active$.next(false);
    ticks$.next(1);

    expect(fetch).not.toHaveBeenCalled();
  });

  it('refetches immediately when selection changes while active, flagged as an activation emission', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(true);
    selection$.next('s1');
    selection$.next('s2');

    expect(fetch).toHaveBeenLastCalledWith('s2');
    expect(emissions[emissions.length - 1]).toEqual({ value: 'data', isActivation: true });
  });

  it('cancels a stale in-flight request when the selection changes before it resolves', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const s1Response = new Subject<string>();
    const s2Response = new Subject<string>();
    const fetch = vi.fn<Fetch>().mockImplementation((s: string) => (s === 's1' ? s1Response : s2Response));
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(true);
    selection$.next('s1');
    selection$.next('s2');
    s2Response.next('fresh s2 data');
    // s1's response arrives late, after the switch to s2 — switchMap should have unsubscribed
    // from it already, so this must produce no emission at all, not a second "stale" one.
    s1Response.next('stale s1 data');

    expect(emissions).toEqual([{ value: 'fresh s2 data', isActivation: true }]);
  });

  it('fetches again after reactivation when selection$ replays its current value', () => {
    // Uses a BehaviorSubject rather than the plain Subject the other tests use: the doc-comment
    // precondition on selection$ is specifically about replaying its current value to a fresh
    // subscription, which only a replaying source (BehaviorSubject/ReplaySubject(1), or
    // toObservable() in real call sites) provides.
    const active$ = new Subject<boolean>();
    const selection$ = new BehaviorSubject<string>('s1');
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(true);
    expect(fetch).toHaveBeenCalledTimes(1);

    active$.next(false);
    fetch.mockClear();

    active$.next(true);

    expect(fetch).toHaveBeenCalledWith('s1');
    expect(emissions[emissions.length - 1]).toEqual({ value: 'data', isActivation: true });
  });

  it('cancels a stale in-flight request when a poll tick fires before it resolves', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const firstResponse = new Subject<string>();
    const secondResponse = new Subject<string>();
    const fetch = vi
      .fn<Fetch>()
      .mockReturnValueOnce(firstResponse)
      .mockReturnValueOnce(secondResponse);
    const emissions: PollEmission<string>[] = [];
    activeGatedPoll(active$, selection$, ticks$, fetch).subscribe((e) => emissions.push(e));

    active$.next(true);
    selection$.next('s1');
    // A real tick fires before the activation fetch resolves — the ticks$-level switchMap
    // should cancel the first (activation) fetch in favor of this one.
    ticks$.next(1);
    secondResponse.next('tick data');
    // The stale activation response arrives late — it must produce no emission.
    firstResponse.next('stale activation data');

    expect(emissions).toEqual([{ value: 'tick data', isActivation: false }]);
  });

  it('stops fetching once the returned observable is unsubscribed', () => {
    const active$ = new Subject<boolean>();
    const selection$ = new Subject<string>();
    const ticks$ = new Subject<number>();
    const fetch = vi.fn<Fetch>().mockReturnValue(of('data'));
    const subscription = activeGatedPoll(active$, selection$, ticks$, fetch).subscribe();

    active$.next(true);
    selection$.next('s1');
    fetch.mockClear();
    subscription.unsubscribe();
    ticks$.next(1);

    expect(fetch).not.toHaveBeenCalled();
  });
});
