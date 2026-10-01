import { TestBed } from '@angular/core/testing';
import { PollingService } from './polling.service';

describe('PollingService', () => {
  let localStorageStub: Record<string, string>;

  beforeEach(() => {
    localStorageStub = {};
    vi.spyOn(window.localStorage, 'getItem').mockImplementation((key) => localStorageStub[key] ?? null);
    vi.spyOn(window.localStorage, 'setItem').mockImplementation((key, value) => {
      localStorageStub[key] = value;
    });
    TestBed.configureTestingModule({ providers: [PollingService] });
  });

  it('defaults to a 5 second interval when localStorage has nothing stored', () => {
    const service = TestBed.inject(PollingService);
    expect(service.intervalSeconds()).toBe(5);
  });

  it('restores a previously stored interval from localStorage', () => {
    localStorageStub['fatrat.webui.pollIntervalSeconds'] = '15';
    const service = TestBed.inject(PollingService);
    expect(service.intervalSeconds()).toBe(15);
  });

  it('ignores a corrupt stored value and falls back to the default', () => {
    localStorageStub['fatrat.webui.pollIntervalSeconds'] = 'not-a-number';
    const service = TestBed.inject(PollingService);
    expect(service.intervalSeconds()).toBe(5);
  });

  it('setIntervalSeconds() updates the signal and persists to localStorage', () => {
    const service = TestBed.inject(PollingService);
    service.setIntervalSeconds(30);
    expect(service.intervalSeconds()).toBe(30);
    expect(localStorageStub['fatrat.webui.pollIntervalSeconds']).toBe('30');
  });

  it('rejects an interval outside the allowed set and keeps the previous value', () => {
    const service = TestBed.inject(PollingService);
    service.setIntervalSeconds(7);
    expect(service.intervalSeconds()).toBe(5);
  });

  it('ticks$ emits repeatedly at the configured interval', () => {
    vi.useFakeTimers();
    const service = TestBed.inject(PollingService);
    const ticks: number[] = [];
    const sub = service.ticks$.subscribe((n) => ticks.push(n));

    vi.advanceTimersByTime(5000);
    expect(ticks.length).toBe(1);
    vi.advanceTimersByTime(10000);
    expect(ticks.length).toBe(3);

    sub.unsubscribe();
    vi.useRealTimers();
  });

  it('ticks$ cancels the pending wait and restarts from the new interval when it changes', () => {
    // Numbers are deliberately chosen so a buggy implementation that keeps the
    // original 5s timer running (rather than cancelling it) would fire at
    // t=5000 and this test would catch it: we check at t=5500, a point where
    // the ORIGINAL 5s timer would already have fired but a correctly RESTARTED
    // 2s timer (started at t=4000) would not yet have fired (due at t=6000).
    vi.useFakeTimers();
    const service = TestBed.inject(PollingService);
    const ticks: number[] = [];
    const sub = service.ticks$.subscribe((n) => ticks.push(n));

    vi.advanceTimersByTime(4000);
    expect(ticks.length).toBe(0);

    service.setIntervalSeconds(2);
    vi.advanceTimersByTime(1500); // t=5500: original 5s timer would have fired by now if not cancelled
    expect(ticks.length).toBe(0); // proves the old timer was actually cancelled, not just ignored

    vi.advanceTimersByTime(500); // t=6000: the restarted 2s timer (started at t=4000) fires now
    expect(ticks.length).toBe(1);

    sub.unsubscribe();
    vi.useRealTimers();
  });

  it('a subscriber that joins after an interval change sees the new interval, not the construction-time default', () => {
    vi.useFakeTimers();
    const service = TestBed.inject(PollingService); // starts at 5s
    service.setIntervalSeconds(2); // change before anyone has subscribed to ticks$ yet

    const ticks: number[] = [];
    const sub = service.ticks$.subscribe((n) => ticks.push(n)); // late subscriber

    vi.advanceTimersByTime(2000);
    expect(ticks.length).toBe(1); // proves it used the 2s interval, not a stale 5s one

    sub.unsubscribe();
    vi.useRealTimers();
  });

  it('two subscribers to ticks$ at different times stay in lockstep instead of each running an independently-timed interval', () => {
    // Before the fix, ticks$ is a cold observable: every .subscribe() re-executes the whole
    // pipe (including a fresh `interval()`), so a second subscriber joining mid-stream gets its
    // own independently-timed sequence instead of sharing the first subscriber's timer. This
    // mirrors activeGatedPoll's real usage, where each data stream in a component subscribes to
    // ticks$ separately and a selection change tears down and resubscribes to it.
    vi.useFakeTimers();
    const service = TestBed.inject(PollingService); // 5s interval

    const firstTicks: number[] = [];
    const sub1 = service.ticks$.subscribe((n) => firstTicks.push(n));

    // Let the first subscriber's timer run partway, then join a second subscriber mid-stream.
    vi.advanceTimersByTime(3000);
    const secondTicks: number[] = [];
    const sub2 = service.ticks$.subscribe((n) => secondTicks.push(n));

    // At t=5000 (2000ms after the second subscriber joined), the shared timer fires: both
    // subscribers should see it. A cold implementation would give the second subscriber its own
    // fresh 5s timer starting at t=3000, so it would NOT have fired yet at t=5000.
    vi.advanceTimersByTime(2000);
    expect(firstTicks.length).toBe(1);
    expect(secondTicks.length).toBe(1);

    sub1.unsubscribe();
    sub2.unsubscribe();
    vi.useRealTimers();
  });

  it('setIntervalSeconds() is a no-op when called with the already-current value', () => {
    vi.useFakeTimers();
    const service = TestBed.inject(PollingService); // 5s
    const ticks: number[] = [];
    const sub = service.ticks$.subscribe((n) => ticks.push(n));

    vi.advanceTimersByTime(3000);
    service.setIntervalSeconds(5); // same as current — should not reset the pending wait
    vi.advanceTimersByTime(2000); // total 5000ms from start
    expect(ticks.length).toBe(1); // fires at the original t=5000, undisturbed by the no-op call

    sub.unsubscribe();
    vi.useRealTimers();
  });
});
