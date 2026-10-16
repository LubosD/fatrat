import { ConnectionHealth } from '../core/connection-health.service';
import { Injectable, inject, signal } from '@angular/core';
import { startWith } from 'rxjs/operators';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';
import { Queue } from '../core/api/models';

@Injectable({ providedIn: 'root' })
export class QueuesStore {
  private readonly transferApi = inject(TransferApi);
  private readonly polling = inject(PollingService);
  private readonly health = inject(ConnectionHealth);

  readonly queues = signal<Queue[]>([]);
  readonly selectedQueueUuid = signal<string | null>(null);

  constructor() {
    this.polling.ticks$.pipe(startWith(0)).subscribe(() => this.refresh());
  }

  selectQueue(uuid: string): void {
    this.selectedQueueUuid.set(uuid);
  }

  /** Triggers an immediate fetch, bypassing the poll interval — call after a mutating action so the UI updates right away. */
  refreshNow(): void {
    this.refresh();
  }

  private refresh(): void {
    this.health.track('queues', this.transferApi.getQueues()).subscribe({
      next: (queues) => {
        this.queues.set(queues);

        const current = this.selectedQueueUuid();
        const stillExists = current !== null && queues.some((q) => q.uuid === current);
        if (!stillExists) {
          this.selectedQueueUuid.set(queues.length > 0 ? queues[0].uuid : null);
        }
      },
      error: () => {}, // reported through ConnectionHealth; keep showing the last known queues
    });
  }
}
