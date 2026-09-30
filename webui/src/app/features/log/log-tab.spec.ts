import { signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { LogTab } from './log-tab';
import { LogService } from '../../core/log.service';
import { TransfersStore } from '../../state/transfers.store';
import { PollingService } from '../../core/polling.service';

// selectedUuids is backed by a REAL signal(), not a plain closure — see the identical comment in
// speed-graphs-tab.spec.ts for why (LogTab's selectedTransferUuid computed() needs an actual
// Signal to track, or a test that changes the "selection" after first read has no effect).
function configure(
  selectedUuids: string[] = [],
  getGlobalLog = vi.fn().mockReturnValue(of('')),
  getTransferLog = vi.fn().mockReturnValue(of('')),
  ticks$ = new Subject<number>(),
) {
  const selectedUuidsSignal = signal(selectedUuids);
  TestBed.configureTestingModule({
    imports: [LogTab],
    providers: [
      { provide: LogService, useValue: { getGlobalLog, getTransferLog } },
      { provide: TransfersStore, useValue: { selectedUuids: selectedUuidsSignal } },
      { provide: PollingService, useValue: { ticks$ } },
    ],
  });
  return { getGlobalLog, getTransferLog, ticks$, selectedUuidsSignal };
}

describe('LogTab', () => {
  it('does not fetch anything while inactive', () => {
    const { getGlobalLog, getTransferLog } = configure(['t1']);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();

    expect(getGlobalLog).not.toHaveBeenCalled();
    expect(getTransferLog).not.toHaveBeenCalled();
  });

  it('fetches the global log on activation, and the transfer log when exactly one transfer is selected', () => {
    const { getGlobalLog, getTransferLog } = configure(['t1'], vi.fn().mockReturnValue(of('global')), vi.fn().mockReturnValue(of('transfer')));
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    expect(getGlobalLog).toHaveBeenCalled();
    expect(getTransferLog).toHaveBeenCalledWith('t1');
    expect(fixture.componentInstance['globalLog']()).toBe('global');
    expect(fixture.componentInstance['transferLog']()).toBe('transfer');
  });

  it('shows a placeholder instead of the transfer log when selection is not exactly one', () => {
    configure([]);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Select a single transfer');
  });

  it('refetches on every poll tick while active, and stops on deactivation', () => {
    const { getGlobalLog, ticks$ } = configure(['t1']);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    expect(getGlobalLog).toHaveBeenCalledTimes(1);

    ticks$.next(1);
    expect(getGlobalLog).toHaveBeenCalledTimes(2);

    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();
    ticks$.next(2);
    expect(getGlobalLog).toHaveBeenCalledTimes(2);
  });

  it('ignores a stale transfer-log response for a transfer that is no longer selected', () => {
    const responseSubject = new Subject<string>();
    const getTransferLog = vi.fn().mockReturnValue(responseSubject);
    const { selectedUuidsSignal } = configure(['t1'], vi.fn().mockReturnValue(of('')), getTransferLog);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();

    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    responseSubject.next('stale t1 log');

    expect(fixture.componentInstance['transferLog']()).toBe('');
  });

  it('scrolls to bottom on activation but not on a subsequent poll-triggered refresh', () => {
    const { ticks$ } = configure(['t1']);
    const fixture = TestBed.createComponent(LogTab);
    const scrollToBottom = vi.spyOn(fixture.componentInstance as any, 'scrollToBottom');
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    expect(scrollToBottom).toHaveBeenCalledTimes(2); // once for global log, once for transfer log

    scrollToBottom.mockClear();
    ticks$.next(1);
    expect(scrollToBottom).not.toHaveBeenCalled();
  });
});
