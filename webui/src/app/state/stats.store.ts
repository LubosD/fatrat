import { ConnectionHealth } from '../core/connection-health.service';
import { Injectable, computed, inject, signal } from '@angular/core';
import { startWith } from 'rxjs/operators';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';
import { QueuesStore } from './queues.store';
import { SpeedLimits } from '../core/api/models';

const ZERO: SpeedLimits = { down: 0, up: 0 };

@Injectable({ providedIn: 'root' })
export class StatsStore {
  private readonly transferApi = inject(TransferApi);
  private readonly polling = inject(PollingService);
  private readonly health = inject(ConnectionHealth);
  private readonly queuesStore = inject(QueuesStore);

  /** Combined rate of every queue, in bytes/s. */
  readonly total = signal<SpeedLimits>(ZERO);
  private readonly perQueue = signal<{ [queueUuid: string]: SpeedLimits }>({});

  /** Rate of the currently selected queue, in bytes/s. */
  readonly selectedQueue = computed<SpeedLimits>(() => {
    const uuid = this.queuesStore.selectedQueueUuid();
    return (uuid !== null && this.perQueue()[uuid]) || ZERO;
  });

  constructor() {
    this.polling.ticks$.pipe(startWith(0)).subscribe(() => this.refresh());
  }

  private refresh(): void {
    this.health.track('stats', this.transferApi.getStats()).subscribe({
      next: (stats) => {
        this.total.set(stats.total);
        this.perQueue.set(stats.queues);
      },
      error: () => {}, // reported through ConnectionHealth; keep showing the last known rates
    });
  }
}
