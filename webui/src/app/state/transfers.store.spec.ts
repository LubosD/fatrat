import { signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { TransfersStore, availableTransferActions } from './transfers.store';
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
    completionCommand: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
  };
}

describe('TransfersStore', () => {
  let ticks$: Subject<number>;
  // A real signal: the store derives an Observable from it via toObservable(), which a plain
  // function stub would never drive.
  let selectedQueueUuid: ReturnType<typeof signal<string | null>>;
  let getQueueTransfers: ReturnType<typeof vi.fn>;

  /** toObservable() emits from an effect, so flush once to deliver the initial selection. */
  function createStore(): TransfersStore {
    const store = TestBed.inject(TransfersStore);
    TestBed.tick();
    return store;
  }

  beforeEach(() => {
    ticks$ = new Subject<number>();
    selectedQueueUuid = signal<string | null>(null);
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
    const store = createStore();
    expect(store.transfers()).toEqual([]);
    expect(getQueueTransfers).not.toHaveBeenCalled();
  });

  it('fetches transfers for the selected queue once one is selected', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockReturnValue(of([makeTransfer('t1', 'file.zip')]));
    const store = createStore();
    ticks$.next(1);
    expect(getQueueTransfers).toHaveBeenCalledWith('q1');
    expect(store.transfers()).toEqual([makeTransfer('t1', 'file.zip')]);
  });

  it('re-fetches on every tick for the currently selected queue', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers
      .mockReturnValueOnce(of([makeTransfer('t1', 'a')]))
      .mockReturnValueOnce(of([makeTransfer('t1', 'a'), makeTransfer('t2', 'b')]));
    const store = createStore();
    // The initial selection already triggers one fetch on creation (the next tests rely on
    // this too) — that call consumes the first mockReturnValueOnce.
    expect(store.transfers().length).toBe(1);
    ticks$.next(1);
    expect(store.transfers().length).toBe(2);
  });

  it('clears transfers when the selected queue changes to null', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockReturnValue(of([makeTransfer('t1', 'a')]));
    const store = createStore();
    // Construction already fetched once (see note above).
    expect(store.transfers().length).toBe(1);
    expect(getQueueTransfers).toHaveBeenCalledTimes(1);

    selectedQueueUuid.set(null);

    TestBed.tick();
    ticks$.next(1);
    expect(store.transfers()).toEqual([]);
    // With no selected queue the store clears transfers without calling the API,
    // so the call count stays at 1.
    expect(getQueueTransfers).toHaveBeenCalledTimes(1);
  });

  it('isSelected()/toggleSelection() track selection membership', () => {
    const store = createStore();
    expect(store.isSelected('t1')).toBe(false);
    store.toggleSelection('t1');
    expect(store.isSelected('t1')).toBe(true);
    store.toggleSelection('t1');
    expect(store.isSelected('t1')).toBe(false);
  });

  it('toggleSelection() adds and removes independently of other selected uuids', () => {
    const store = createStore();
    store.toggleSelection('t1');
    store.toggleSelection('t2');
    expect(store.selectedUuids()).toEqual(['t1', 't2']);
    store.toggleSelection('t1');
    expect(store.selectedUuids()).toEqual(['t2']);
  });

  it('selectOnly() replaces the entire selection with a single uuid', () => {
    const store = createStore();
    store.toggleSelection('t1');
    store.toggleSelection('t2');
    store.selectOnly('t3');
    expect(store.selectedUuids()).toEqual(['t3']);
  });

  it('clearSelection() empties the selection', () => {
    const store = createStore();
    store.toggleSelection('t1');
    store.clearSelection();
    expect(store.selectedUuids()).toEqual([]);
  });

  it('fetches immediately when the selected queue changes, without waiting for a tick', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockImplementation((uuid: string) => of([makeTransfer(uuid + '-t', 'x')]));
    const store = createStore();
    expect(store.transfers()[0].uuid).toBe('q1-t');

    selectedQueueUuid.set('q2');
    TestBed.tick();
    expect(getQueueTransfers).toHaveBeenLastCalledWith('q2');
    expect(store.transfers()[0].uuid).toBe('q2-t');
  });

  it('ignores a late response for a queue that is no longer selected', () => {
    const q1Response = new Subject<Transfer[]>();
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockImplementation((uuid: string) => (uuid === 'q1' ? q1Response : of([makeTransfer('q2-t', 'x')])));
    const store = createStore();

    selectedQueueUuid.set('q2');
    TestBed.tick();
    q1Response.next([makeTransfer('q1-t', 'stale')]);
    expect(store.transfers().map((t) => t.uuid)).toEqual(['q2-t']);
  });

  it('refreshNow() fetches again without waiting for a tick', () => {
    selectedQueueUuid.set('q1');
    const store = createStore();
    expect(getQueueTransfers).toHaveBeenCalledTimes(1);
    store.refreshNow();
    expect(getQueueTransfers).toHaveBeenCalledTimes(2);
  });

  describe('Qt-style extended selection', () => {
    function storeWith(...uuids: string[]) {
      selectedQueueUuid.set('q1');
      getQueueTransfers.mockReturnValue(of(uuids.map((u) => makeTransfer(u, u))));
      return createStore();
    }

    it('plain click selects only the clicked row', () => {
      const store = storeWith('t1', 't2', 't3');
      store.select('t1');
      store.select('t3');
      expect(store.selectedUuids()).toEqual(['t3']);
      expect(store.currentUuid()).toBe('t3');
    });

    it('Ctrl+click toggles a row without touching the rest', () => {
      const store = storeWith('t1', 't2', 't3');
      store.select('t1');
      store.select('t3', { ctrl: true });
      expect(store.selectedUuids()).toEqual(['t1', 't3']);
      store.select('t1', { ctrl: true });
      expect(store.selectedUuids()).toEqual(['t3']);
    });

    it('Shift+click selects the range from the anchor, in either direction, keeping the anchor', () => {
      const store = storeWith('t1', 't2', 't3', 't4');
      store.select('t2');
      store.select('t4', { shift: true });
      expect(store.selectedUuids()).toEqual(['t2', 't3', 't4']);
      store.select('t1', { shift: true });
      expect(store.selectedUuids()).toEqual(['t1', 't2']);
    });

    it('Ctrl+Shift+click adds the range to the existing selection', () => {
      const store = storeWith('t1', 't2', 't3', 't4', 't5');
      store.select('t1');
      store.select('t3', { ctrl: true });
      store.select('t5', { ctrl: true, shift: true });
      expect(store.selectedUuids()).toEqual(['t1', 't3', 't4', 't5']);
    });

    it('Shift+click with no anchor behaves like a plain click', () => {
      const store = storeWith('t1', 't2');
      store.select('t2', { shift: true });
      expect(store.selectedUuids()).toEqual(['t2']);
    });

    it('selectAll() selects every transfer in table order', () => {
      const store = storeWith('t1', 't2', 't3');
      store.selectAll();
      expect(store.selectedUuids()).toEqual(['t1', 't2', 't3']);
    });

    it('moveCurrent() starts at the first/last row, clamps, and extends with Shift', () => {
      const store = storeWith('t1', 't2', 't3');
      store.moveCurrent(1);
      expect(store.selectedUuids()).toEqual(['t1']);
      store.moveCurrent(1, { shift: true });
      store.moveCurrent(1, { shift: true });
      expect(store.selectedUuids()).toEqual(['t1', 't2', 't3']);
      store.moveCurrent(Infinity);
      expect(store.selectedUuids()).toEqual(['t3']);
      store.moveCurrent(-Infinity);
      expect(store.selectedUuids()).toEqual(['t1']);
    });

    it('moveCurrent() from nothing with a negative delta starts at the last row', () => {
      const store = storeWith('t1', 't2', 't3');
      store.moveCurrent(-1);
      expect(store.selectedUuids()).toEqual(['t3']);
    });
  });

  it('prunes selected uuids that disappear from a refresh', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers
      .mockReturnValueOnce(of([makeTransfer('t1', 'a'), makeTransfer('t2', 'b')]))
      .mockReturnValueOnce(of([makeTransfer('t1', 'a')]));
    const store = createStore();
    // Construction fetch (first mock) populated transfers = [t1, t2].
    store.toggleSelection('t1');
    store.toggleSelection('t2');
    expect(store.selectedUuids()).toEqual(['t1', 't2']);

    ticks$.next(1); // consumes the second mock -> transfers = [t1]
    expect(store.selectedUuids()).toEqual(['t1']);
  });

  it('clears the selection when the selected queue becomes null', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockReturnValue(of([makeTransfer('t1', 'a')]));
    const store = createStore();
    store.toggleSelection('t1');
    expect(store.selectedUuids()).toEqual(['t1']);

    selectedQueueUuid.set(null);

    TestBed.tick();
    ticks$.next(1);
    expect(store.selectedUuids()).toEqual([]);
  });

  it('refreshNow() triggers an immediate fetch outside the poll cycle', () => {
    selectedQueueUuid.set('q1');
    getQueueTransfers.mockReturnValue(of([makeTransfer('t1', 'a')]));
    const store = createStore();
    expect(getQueueTransfers).toHaveBeenCalledTimes(1); // construction
    store.refreshNow();
    expect(getQueueTransfers).toHaveBeenCalledTimes(2);
  });
});

describe('availableTransferActions', () => {
  function t(uuid: string, state: Transfer['state']): Transfer {
    return { ...makeTransfer(uuid, uuid), state };
  }

  it('enables nothing selection-dependent with an empty selection', () => {
    expect(availableTransferActions([t('a', 'Paused')], [])).toEqual({
      resume: false,
      forceResume: false,
      pause: false,
      delete: false,
      moveUp: false,
      moveDown: false,
      properties: false,
    });
  });

  it('enables Resume only if some selected transfer is Paused, Failed or ForcedActive', () => {
    const transfers = [t('a', 'Active'), t('b', 'Waiting'), t('c', 'Failed'), t('d', 'Completed'), t('e', 'ForcedActive'), t('f', 'Paused')];
    expect(availableTransferActions(transfers, ['a', 'b', 'd']).resume).toBe(false);
    expect(availableTransferActions(transfers, ['a', 'c']).resume).toBe(true);
    expect(availableTransferActions(transfers, ['d', 'e']).resume).toBe(true);
    expect(availableTransferActions(transfers, ['f']).resume).toBe(true);
  });

  it('enables Pause only if some selected transfer is not already Paused', () => {
    const transfers = [t('a', 'Paused'), t('b', 'Completed')];
    expect(availableTransferActions(transfers, ['a']).pause).toBe(false);
    expect(availableTransferActions(transfers, ['a', 'b']).pause).toBe(true);
  });

  it('enables Move Up/Down only if some selected transfer can still move that way', () => {
    const transfers = [t('a', 'Active'), t('b', 'Active'), t('c', 'Active')];
    expect(availableTransferActions(transfers, ['a'])).toMatchObject({ moveUp: false, moveDown: true });
    expect(availableTransferActions(transfers, ['c'])).toMatchObject({ moveUp: true, moveDown: false });
    expect(availableTransferActions(transfers, ['a', 'c'])).toMatchObject({ moveUp: true, moveDown: true });
  });

  it('enables Properties for one or more selected transfers, not for none', () => {
    const transfers = [t('a', 'Active'), t('b', 'Active')];
    expect(availableTransferActions(transfers, ['a']).properties).toBe(true);
    expect(availableTransferActions(transfers, ['a', 'b']).properties).toBe(true);
    expect(availableTransferActions(transfers, []).properties).toBe(false);
  });
});
