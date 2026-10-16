import { TestBed } from '@angular/core/testing';
import { of, Subject } from 'rxjs';
import { signal } from '@angular/core';
import { StatsStore } from './stats.store';
import { QueuesStore } from './queues.store';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';

describe('StatsStore', () => {
  let ticks$: Subject<number>;
  let getStats: ReturnType<typeof vi.fn>;
  const selected = signal<string | null>('q1');

  beforeEach(() => {
    ticks$ = new Subject<number>();
    selected.set('q1');
    getStats = vi.fn().mockReturnValue(
      of({ total: { down: 300, up: 30 }, queues: { q1: { down: 100, up: 10 }, q2: { down: 200, up: 20 } } }),
    );
    TestBed.configureTestingModule({
      providers: [
        StatsStore,
        { provide: TransferApi, useValue: { getStats } },
        { provide: PollingService, useValue: { ticks$ } },
        { provide: QueuesStore, useValue: { selectedQueueUuid: selected } },
      ],
    });
  });

  it('fetches immediately and exposes the total', () => {
    const store = TestBed.inject(StatsStore);
    expect(getStats).toHaveBeenCalledTimes(1);
    expect(store.total()).toEqual({ down: 300, up: 30 });
  });

  it('refetches on every poll tick', () => {
    TestBed.inject(StatsStore);
    ticks$.next(1);
    expect(getStats).toHaveBeenCalledTimes(2);
  });

  it('follows the selected queue and falls back to zero', () => {
    const store = TestBed.inject(StatsStore);
    expect(store.selectedQueue()).toEqual({ down: 100, up: 10 });
    selected.set('q2');
    expect(store.selectedQueue()).toEqual({ down: 200, up: 20 });
    selected.set(null);
    expect(store.selectedQueue()).toEqual({ down: 0, up: 0 });
  });
});
