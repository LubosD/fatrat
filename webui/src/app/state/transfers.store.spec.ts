import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { TransfersStore } from './transfers.store';
import { QueuesStore } from './queues.store';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';
import { Transfer } from '../core/api/models';

function makeTransfer(uuid: string, name: string): Transfer {
  return {
    uuid,
    name,
    state: 'Active',
    class: 'HttpDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/' + name,
    dataPathIsDir: false,
    total: 100,
    done: 50,
    comment: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
  };
}

describe('TransfersStore', () => {
  let ticks$: Subject<number>;
  let selectedQueueUuid: ReturnType<typeof signalStub>;
  let getQueueTransfers: ReturnType<typeof vi.fn>;

  function signalStub(initial: string | null) {
    let value = initial;
    const fn = (() => value) as { (): string | null; set: (v: string | null) => void };
    fn.set = (v: string | null) => (value = v);
    return fn;
  }

  beforeEach(() => {
    ticks$ = new Subject<number>();
    selectedQueueUuid = signalStub(null);
    getQueueTransfers = vi.fn().mockReturnValue(of([]));
    TestBed.configureTestingModule({
      providers: [
        TransfersStore,
        { provide: TransferApi, useValue: { getQueueTransfers } },
        { provide: PollingService, useValue: { ticks$, intervalSeconds: () => 5 } },
        { provide: QueuesStore, useValue: { selectedQueueUuid } },
      ],
    });
  });

  it('starts with an empty transfers list when no queue is selected', () => {
    const store = TestBed.inject(TransfersStore);
    expect(store.transfers()).toEqual([]);
    expect(getQueueTransfers).not.toHaveBeenCalled();
  });

  it('fetches transfers for the selected queue once one is selected', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockReturnValue(of([makeTransfer('t1', 'file.zip')]));
    const store = TestBed.inject(TransfersStore);
    ticks$.next(1);
    expect(getQueueTransfers).toHaveBeenCalledWith('q1');
    expect(store.transfers()).toEqual([makeTransfer('t1', 'file.zip')]);
  });

  it('re-fetches on every tick for the currently selected queue', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers
      .mockReturnValueOnce(of([makeTransfer('t1', 'a')]))
      .mockReturnValueOnce(of([makeTransfer('t1', 'a'), makeTransfer('t2', 'b')]));
    const store = TestBed.inject(TransfersStore);
    // The constructor's startWith(0) already triggers one synchronous fetch on
    // creation (this is what the first test in this file, "fetches transfers
    // for the selected queue once one is selected," relies on too) — that
    // construction-time call consumes the first mockReturnValueOnce.
    expect(store.transfers().length).toBe(1);
    ticks$.next(1);
    expect(store.transfers().length).toBe(2);
  });

  it('clears transfers when the selected queue changes to null', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockReturnValue(of([makeTransfer('t1', 'a')]));
    const store = TestBed.inject(TransfersStore);
    // Construction already fetched once (see note above).
    expect(store.transfers().length).toBe(1);
    expect(getQueueTransfers).toHaveBeenCalledTimes(1);

    selectedQueueUuid.set(null);
    ticks$.next(1);
    expect(store.transfers()).toEqual([]);
    // refresh() short-circuits to transfers.set([]) without calling the API
    // when there's no selected queue, so the call count stays at 1.
    expect(getQueueTransfers).toHaveBeenCalledTimes(1);
  });
});
