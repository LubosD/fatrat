import { TestBed } from '@angular/core/testing';
import { delay, of, Subject } from 'rxjs';
import { signal } from '@angular/core';
import { SpeedGraphsTab } from './speed-graphs-tab';
import { TransferApi } from '../../core/api/transfer-api';
import { TransfersStore } from '../../state/transfers.store';
import { QueuesStore } from '../../state/queues.store';
import { PollingService } from '../../core/polling.service';

// selectedUuids/selectedQueueUuid are backed by REAL signal()s, not plain closures — SpeedGraphsTab
// derives selectedTransferUuid via computed() and feeds both through toObservable(), both of which
// only track reactive dependencies through actual Signal reads.
function configure(
  selectedUuids: string[] = [],
  selectedQueueUuid: string | null = 'q1',
  getTransferSpeedGraph = vi.fn().mockReturnValue(of([])),
  getQueueSpeedGraph = vi.fn().mockReturnValue(of([])),
  ticks$ = new Subject<number>(),
) {
  const selectedUuidsSignal = signal(selectedUuids);
  const selectedQueueUuidSignal = signal(selectedQueueUuid);
  TestBed.configureTestingModule({
    imports: [SpeedGraphsTab],
    providers: [
      { provide: TransferApi, useValue: { getTransferSpeedGraph, getQueueSpeedGraph } },
      { provide: TransfersStore, useValue: { selectedUuids: selectedUuidsSignal } },
      { provide: QueuesStore, useValue: { selectedQueueUuid: selectedQueueUuidSignal } },
      { provide: PollingService, useValue: { ticks$ } },
    ],
  });
  return { getTransferSpeedGraph, getQueueSpeedGraph, ticks$, selectedUuidsSignal, selectedQueueUuidSignal };
}

describe('SpeedGraphsTab', () => {
  it('does not fetch anything while inactive', async () => {
    const { getTransferSpeedGraph, getQueueSpeedGraph } = configure(['t1']);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(getTransferSpeedGraph).not.toHaveBeenCalled();
    expect(getQueueSpeedGraph).not.toHaveBeenCalled();
  });

  it('fetches both graphs immediately on activation', async () => {
    const { getTransferSpeedGraph, getQueueSpeedGraph } = configure(['t1'], 'q1');
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(getTransferSpeedGraph).toHaveBeenCalledWith('t1');
    expect(getQueueSpeedGraph).toHaveBeenCalledWith('q1');
  });

  it('shows a placeholder instead of the transfer chart when selection is not exactly one', async () => {
    configure([]);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Select a single transfer');
  });

  it('shows a placeholder instead of the queue chart when no queue is selected', async () => {
    configure(['t1'], null);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Select a queue');
  });

  it('refetches on every poll tick while active, and stops on deactivation', async () => {
    const { getTransferSpeedGraph, ticks$ } = configure(['t1'], 'q1');
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(1);

    ticks$.next(1);
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(2);

    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();
    await fixture.whenStable();
    ticks$.next(2);
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(2);
  });

  it('refetches the transfer graph when the selected transfer changes while active, without waiting for a poll tick', async () => {
    const getTransferSpeedGraph = vi.fn().mockReturnValue(of([]));
    const { selectedUuidsSignal } = configure(['t1'], 'q1', getTransferSpeedGraph);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(1);
    expect(getTransferSpeedGraph).toHaveBeenCalledWith('t1');

    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(getTransferSpeedGraph).toHaveBeenCalledWith('t2');
    expect(getTransferSpeedGraph.mock.calls.length).toBeGreaterThanOrEqual(2);
  });

  it('applies the freshest response even when an older request for a previous selection resolves late', async () => {
    const t1Response = new Subject<{ down: number; up: number }[]>();
    const t2Response = new Subject<{ down: number; up: number }[]>();
    const getTransferSpeedGraph = vi
      .fn()
      .mockImplementation((uuid: string) => (uuid === 't1' ? t1Response : t2Response));
    const { selectedUuidsSignal } = configure(['t1'], 'q1', getTransferSpeedGraph);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();

    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();

    t2Response.next([{ down: 1, up: 2 }]);
    // t1's response arrives late, after the switch to t2 — activeGatedPoll's switchMap already
    // unsubscribed from it, so this must have no effect at all (not just be "ignored").
    t1Response.next([{ down: 999, up: 999 }]);

    expect(fixture.componentInstance['transferSamples']()).toEqual([{ down: 1, up: 2 }]);
  });

  it('integration: still fetches the fresh selection after a real poll tick has elapsed, using the real (shareReplay-backed) PollingService', async () => {
    // Mirrors the equivalent test in log-tab.spec.ts — PollingService.ticks$ is NOT mocked here
    // (unlike every other test in this file): it's the real, production
    // shareReplay({ bufferSize: 1, refCount: false }) observable. Fetches are genuinely async
    // (delay(1)) rather than of(...), since the activeGatedPoll/ticks$-shareReplay race this
    // guards against only manifests for async fetches. SpeedGraphsTab discards `isActivation`
    // entirely, so it can't exhibit that specific bug — this test instead proves the more basic
    // property that still depends on the same real stack: a selection change after a real tick
    // has elapsed still reliably produces a fetch for the NEW selection, through the real,
    // multicast ticks$, not just through a mocked Subject that happens to behave differently.
    vi.useFakeTimers();
    try {
      const selectedUuidsSignal = signal(['t1']);
      const selectedQueueUuidSignal = signal<string | null>('q1');
      const getTransferSpeedGraph = vi.fn().mockReturnValue(of([{ down: 1, up: 2 }]).pipe(delay(1)));
      const getQueueSpeedGraph = vi.fn().mockReturnValue(of([]).pipe(delay(1)));
      TestBed.configureTestingModule({
        imports: [SpeedGraphsTab],
        providers: [
          { provide: TransferApi, useValue: { getTransferSpeedGraph, getQueueSpeedGraph } },
          { provide: TransfersStore, useValue: { selectedUuids: selectedUuidsSignal } },
          { provide: QueuesStore, useValue: { selectedQueueUuid: selectedQueueUuidSignal } },
          // No PollingService override — use the real, app-wide singleton with its real
          // shareReplay'd ticks$.
        ],
      });
      const fixture = TestBed.createComponent(SpeedGraphsTab);
      fixture.componentRef.setInput('active', true);
      fixture.detectChanges();
      await vi.advanceTimersByTimeAsync(1);
      fixture.detectChanges();

      // Let a real poll tick elapse so PollingService.ticks$'s shareReplay buffer now holds a
      // real tick number — the exact precondition that triggered the original bug.
      await vi.advanceTimersByTimeAsync(5001);
      fixture.detectChanges();
      getTransferSpeedGraph.mockClear();
      getTransferSpeedGraph.mockReturnValue(of([{ down: 9, up: 9 }]).pipe(delay(1)));

      // Change selection: this re-subscribes to the real ticks$ fresh, which synchronously
      // replays the buffered real tick right after activeGatedPoll's startWith(null).
      selectedUuidsSignal.set(['t2']);
      fixture.detectChanges();
      await vi.advanceTimersByTimeAsync(1);
      fixture.detectChanges();

      expect(getTransferSpeedGraph).toHaveBeenCalledWith('t2');
      expect(fixture.componentInstance['transferSamples']()).toEqual([{ down: 9, up: 9 }]);
    } finally {
      vi.useRealTimers();
    }
  });
});
