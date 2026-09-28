import { Injectable, inject } from '@angular/core';
import { TransferApi } from './api/transfer-api';
import { QueuesStore } from '../state/queues.store';
import { Queue } from './api/models';

@Injectable({ providedIn: 'root' })
export class QueueActionsService {
  private readonly transferApi = inject(TransferApi);
  private readonly queuesStore = inject(QueuesStore);

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
}
