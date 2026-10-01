import { signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { LogTab } from './log-tab';
import { LogService } from '../../core/log.service';
import { TransfersStore } from '../../state/transfers.store';
import { PollingService } from '../../core/polling.service';

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
  it('does not fetch anything while inactive', async () => {
    const { getGlobalLog, getTransferLog } = configure(['t1']);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(getGlobalLog).not.toHaveBeenCalled();
    expect(getTransferLog).not.toHaveBeenCalled();
  });

  it('fetches the global log on activation, and the transfer log when exactly one transfer is selected', async () => {
    const { getGlobalLog, getTransferLog } = configure(
      ['t1'],
      vi.fn().mockReturnValue(of('global')),
      vi.fn().mockReturnValue(of('transfer')),
    );
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(getGlobalLog).toHaveBeenCalled();
    expect(getTransferLog).toHaveBeenCalledWith('t1');
    expect(fixture.componentInstance['globalLog']()).toBe('global');
    expect(fixture.componentInstance['transferLog']()).toBe('transfer');
  });

  it('shows a placeholder instead of the transfer log when selection is not exactly one', async () => {
    configure([]);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Select a single transfer');
  });

  it('refetches on every poll tick while active, and stops on deactivation', async () => {
    const { getGlobalLog, ticks$ } = configure(['t1']);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    expect(getGlobalLog).toHaveBeenCalledTimes(1);

    ticks$.next(1);
    expect(getGlobalLog).toHaveBeenCalledTimes(2);

    fixture.componentRef.setInput('active', false);
    fixture.detectChanges();
    await fixture.whenStable();
    ticks$.next(2);
    expect(getGlobalLog).toHaveBeenCalledTimes(2);
  });

  it('refetches the transfer log when the selected transfer changes while active, without waiting for a poll tick', async () => {
    const getTransferLog = vi.fn().mockReturnValue(of(''));
    const { selectedUuidsSignal } = configure(['t1'], vi.fn().mockReturnValue(of('')), getTransferLog);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    expect(getTransferLog).toHaveBeenCalledTimes(1);
    expect(getTransferLog).toHaveBeenCalledWith('t1');

    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(getTransferLog).toHaveBeenCalledWith('t2');
    expect(getTransferLog.mock.calls.length).toBeGreaterThanOrEqual(2);
  });

  it('applies the freshest response even when an older request for a previous selection resolves late', async () => {
    const t1Response = new Subject<string>();
    const t2Response = new Subject<string>();
    const getTransferLog = vi.fn().mockImplementation((uuid: string) => (uuid === 't1' ? t1Response : t2Response));
    const { selectedUuidsSignal } = configure(['t1'], vi.fn().mockReturnValue(of('')), getTransferLog);
    const fixture = TestBed.createComponent(LogTab);
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();

    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();

    t2Response.next('fresh t2 log');
    t1Response.next('stale t1 log');

    expect(fixture.componentInstance['transferLog']()).toBe('fresh t2 log');
  });

  it('scrolls to bottom on activation and on a selection change, but not on a subsequent poll-triggered refresh', async () => {
    const { ticks$, selectedUuidsSignal } = configure(['t1']);
    const fixture = TestBed.createComponent(LogTab);
    const scrollToBottom = vi.spyOn(fixture.componentInstance as any, 'scrollToBottom');
    fixture.componentRef.setInput('active', true);
    fixture.detectChanges();
    await fixture.whenStable();
    expect(scrollToBottom).toHaveBeenCalledTimes(2); // once for global log, once for transfer log

    scrollToBottom.mockClear();
    ticks$.next(1);
    expect(scrollToBottom).not.toHaveBeenCalled();

    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();
    // Selection change re-triggers the transfer-log stream (isActivation: true for it); the
    // global-log stream is unaffected by transfer selection, so only one more scroll call.
    expect(scrollToBottom).toHaveBeenCalledTimes(1);
  });
});
