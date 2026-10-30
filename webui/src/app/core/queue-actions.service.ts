import { Injectable, inject } from '@angular/core';
import { TransferApi } from './api/transfer-api';
import { QueuesStore } from '../state/queues.store';
import { DeleteQueueOptions, Queue, Transfer } from './api/models';
import { TransfersStore } from '../state/transfers.store';
import { ConfirmationService, MessageService } from '@openng/optimus-ui/api';
import { Observable, map, of, switchMap } from 'rxjs';

@Injectable({ providedIn: 'root' })
export class QueueActionsService {
  private readonly transferApi = inject(TransferApi);
  private readonly queuesStore = inject(QueuesStore);
  private readonly transfersStore = inject(TransfersStore);
  private readonly confirmation = inject(ConfirmationService);
  private readonly messages = inject(MessageService);

  createQueue(properties: Partial<Queue>): void {
    this.transferApi.createQueue(properties).subscribe({
      next: () => this.queuesStore.refreshNow(),
      error: (err) => this.showError('Cannot create the queue', err),
    });
  }

  updateQueueProperties(uuid: string, properties: Partial<Queue>): void {
    this.transferApi.setQueueProperties(uuid, properties).subscribe({
      next: () => this.queuesStore.refreshNow(),
      error: (err) => this.showError('Cannot save the queue', err),
    });
  }

  countTransfers(uuid: string): Observable<number> {
    return this.transferApi.getQueueTransfers(uuid).pipe(map((transfers) => transfers.length));
  }

  /** Confirmation for a queue that holds no transfers; non-empty queues go through the delete dialog. */
  confirmDeleteEmptyQueue(queue: Queue): void {
    this.confirmation.confirm({
      header: 'Confirm Delete',
      message: `Delete the empty queue "${queue.name}"?`,
      accept: () => this.deleteQueue(queue, {}),
    });
  }

  deleteQueue(queue: Queue, options: DeleteQueueOptions): void {
    this.transferApi.deleteQueue(queue.uuid, options).subscribe({
      next: () => {
        this.queuesStore.refreshNow();
        this.transfersStore.refreshNow();
      },
      error: (err) => console.error('Failed to delete queue', err),
    });
  }

  /** Mirrors the desktop queue menu: Resume All queues the paused and failed transfers. */
  resumeAll(uuid: string): void {
    this.setAllStates(uuid, 'Waiting', (t) => t.state === 'Paused' || t.state === 'Failed');
  }

  /** Stop All pauses everything that isn't finished. */
  stopAll(uuid: string): void {
    this.setAllStates(uuid, 'Paused', (t) => t.state !== 'Completed');
  }

  private showError(summary: string, err: unknown): void {
    this.messages.add({ severity: 'error', summary, detail: err instanceof Error ? err.message : String(err) });
  }

  private setAllStates(queueUuid: string, state: 'Waiting' | 'Paused', applies: (t: Transfer) => boolean): void {
    this.transferApi
      .getQueueTransfers(queueUuid)
      .pipe(
        map((transfers) => transfers.filter(applies).map((t) => t.uuid)),
        switchMap((uuids) => (uuids.length === 0 ? of(undefined) : this.transferApi.setTransferProperties(uuids, { state }))),
      )
      .subscribe({
        next: () => this.transfersStore.refreshNow(),
        error: (err) => console.error('Failed to update queue transfers', err),
      });
  }
}
