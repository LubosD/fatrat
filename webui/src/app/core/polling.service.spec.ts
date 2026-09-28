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
});
