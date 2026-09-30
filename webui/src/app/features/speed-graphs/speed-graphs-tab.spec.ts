import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { signal } from '@angular/core';
import { SpeedGraphsTab } from './speed-graphs-tab';
import { TransferApi } from '../../core/api/transfer-api';
import { TransfersStore } from '../../state/transfers.store';
import { QueuesStore } from '../../state/queues.store';
import { PollingService } from '../../core/polling.service';

// selectedUuids/selectedQueueUuid are backed by REAL signal()s, not plain closures returning a
// fixed value — SpeedGraphsTab derives selectedTransferUuid via computed(), and Angular's
// computed() only tracks reactive dependencies through actual Signal reads. A plain function
// mock would let the component read an initial value, but computed() would then cache that
// forever (no signal to invalidate it), so a test that tries to change the "selection" after the
// component has already read it once needs a real signal for that change to be observed at all.
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
  it('does not fetch anything while inactive', () => {
    const { getTransferSpeedGraph, getQueueSpeedGraph } = configure(['t1']);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();

    expect(getTransferSpeedGraph).not.toHaveBeenCalled();
    expect(getQueueSpeedGraph).not.toHaveBeenCalled();
  });

  it('fetches both graphs immediately on activation', () => {
    const { getTransferSpeedGraph, getQueueSpeedGraph } = configure(['t1'], 'q1');
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    expect(getTransferSpeedGraph).toHaveBeenCalledWith('t1');
    expect(getQueueSpeedGraph).toHaveBeenCalledWith('q1');
  });

  it('shows a placeholder instead of the transfer chart when selection is not exactly one', () => {
    configure([]);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Select a single transfer');
  });

  it('shows a placeholder instead of the queue chart when no queue is selected', () => {
    configure(['t1'], null);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Select a queue');
  });

  it('refetches on every poll tick while active, and stops on deactivation', () => {
    const { getTransferSpeedGraph, ticks$ } = configure(['t1'], 'q1');
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(1);

    ticks$.next(1);
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(2);

    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();
    ticks$.next(2);
    expect(getTransferSpeedGraph).toHaveBeenCalledTimes(2);
  });

  it('ignores a stale speed-graph response for a transfer that is no longer selected', () => {
    const responseSubject = new Subject<{ down: number; up: number }[]>();
    const getTransferSpeedGraph = vi.fn().mockReturnValue(responseSubject);
    const { selectedUuidsSignal } = configure(['t1'], 'q1', getTransferSpeedGraph);
    const fixture = TestBed.createComponent(SpeedGraphsTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    // Selection changes to a different transfer before t1's response arrives.
    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    responseSubject.next([{ down: 999, up: 999 }]);

    expect(fixture.componentInstance['transferSamples']()).toEqual([]);
  });
});
