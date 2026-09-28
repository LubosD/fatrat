import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { QueuesStore } from './queues.store';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';
import { Queue } from '../core/api/models';

function makeQueue(uuid: string, name: string): Queue {
  return {
    uuid,
    name,
    defaultDirectory: '/downloads',
    moveDirectory: '',
    upAsDown: false,
    transferLimits: { down: 0, up: 0 },
    speedLimits: { down: 0, up: 0 },
  };
}

describe('QueuesStore', () => {
  let ticks$: Subject<number>;
  let getQueues: ReturnType<typeof vi.fn>;

  beforeEach(() => {
    ticks$ = new Subject<number>();
    getQueues = vi.fn().mockReturnValue(of([]));
    TestBed.configureTestingModule({
      providers: [
        QueuesStore,
        { provide: TransferApi, useValue: { getQueues } },
        { provide: PollingService, useValue: { ticks$, intervalSeconds: () => 5 } },
      ],
    });
  });

  it('starts with an empty queues list and no selection', () => {
    const store = TestBed.inject(QueuesStore);
    expect(store.queues()).toEqual([]);
    expect(store.selectedQueueUuid()).toBeNull();
  });

  it('fetches queues immediately on creation, without waiting for a tick', () => {
    getQueues.mockReturnValue(of([makeQueue('q1', 'Default')]));
    const store = TestBed.inject(QueuesStore);
    expect(store.queues()).toEqual([makeQueue('q1', 'Default')]);
  });

  it('re-fetches queues on every PollingService tick', () => {
    getQueues.mockReturnValueOnce(of([makeQueue('q1', 'Default')])).mockReturnValueOnce(of([makeQueue('q1', 'Renamed')]));
    const store = TestBed.inject(QueuesStore);
    expect(store.queues()[0].name).toBe('Default');

    ticks$.next(1);
    expect(store.queues()[0].name).toBe('Renamed');
    expect(getQueues).toHaveBeenCalledTimes(2);
  });

  it('auto-selects the first queue once queues load, if nothing is selected yet', () => {
    getQueues.mockReturnValue(of([makeQueue('q1', 'Default'), makeQueue('q2', 'Second')]));
    const store = TestBed.inject(QueuesStore);
    expect(store.selectedQueueUuid()).toBe('q1');
  });

  it('does not override an existing selection when queues refresh', () => {
    getQueues.mockReturnValue(of([makeQueue('q1', 'Default'), makeQueue('q2', 'Second')]));
    const store = TestBed.inject(QueuesStore);
    store.selectQueue('q2');
    ticks$.next(1);
    expect(store.selectedQueueUuid()).toBe('q2');
  });

  it('clears the selection if the selected queue disappears from a refresh', () => {
    getQueues
      .mockReturnValueOnce(of([makeQueue('q1', 'Default'), makeQueue('q2', 'Second')]))
      .mockReturnValueOnce(of([makeQueue('q1', 'Default')]));
    const store = TestBed.inject(QueuesStore);
    store.selectQueue('q2');
    ticks$.next(1);
    expect(store.selectedQueueUuid()).toBe('q1');
  });

  it('selectQueue() sets the selection directly', () => {
    getQueues.mockReturnValue(of([makeQueue('q1', 'Default'), makeQueue('q2', 'Second')]));
    const store = TestBed.inject(QueuesStore);
    store.selectQueue('q2');
    expect(store.selectedQueueUuid()).toBe('q2');
  });
});
