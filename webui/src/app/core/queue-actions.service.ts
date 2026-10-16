import { Injectable, inject } from '@angular/core';
import { TransferApi } from './api/transfer-api';
import { QueuesStore } from '../state/queues.store';
import { Queue } from './api/models';
import { TransfersStore } from '../state/transfers.store';
import { map, of, switchMap } from 'rxjs';

@Injectable({ providedIn: 'root' })
export class QueueActionsService {
  private readonly transferApi = inject(TransferApi);
  private readonly queuesStore = inject(QueuesStore);
  private readonly transfersStore = inject(TransfersStore);

  createQueue(properties: Partial<Queue>): void {
    this.transferApi.createQueue(properties).subscribe({
      next: () => this.queuesStore.refreshNow(),
      error: (err) => console.error('Failed to create queue', err),
    });
  }

  updateQueueProperties(uuid: string, properties: Partial<Queue>): void {
    this.transferApi.setQueueProperties(uuid, properties).subscribe({
      next: () => this.queuesStore.refreshNow(),
      error: (err) => console.error('Failed to update queue properties', err),
    });
  }

  /** Mirrors the desktop queue menu's "Resume All" / "Stop All". */
  resumeAll(uuid: string): void {
    this.setAllStates(uuid, 'Waiting');
  }

  stopAll(uuid: string): void {
    this.setAllStates(uuid, 'Paused');
  }

  private setAllStates(queueUuid: string, state: 'Waiting' | 'Paused'): void {
    this.transferApi
      .getQueueTransfers(queueUuid)
      .pipe(
        map((transfers) => transfers.map((t) => t.uuid)),
        switchMap((uuids) => (uuids.length === 0 ? of(undefined) : this.transferApi.setTransferProperties(uuids, { state }))),
      )
      .subscribe({
        next: () => this.transfersStore.refreshNow(),
        error: (err) => console.error('Failed to update queue transfers', err),
      });
  }
}
