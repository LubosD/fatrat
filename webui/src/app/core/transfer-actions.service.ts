import { Injectable, inject } from '@angular/core';
import { ConfirmationService } from 'primeng/api';
import { TransferApi } from './api/transfer-api';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { TransferState } from './api/models';

@Injectable({ providedIn: 'root' })
export class TransferActionsService {
  private readonly transferApi = inject(TransferApi);
  private readonly transfersStore = inject(TransfersStore);
  private readonly queuesStore = inject(QueuesStore);
  private readonly confirmation = inject(ConfirmationService);

  resume(): void {
    this.setState('Waiting');
  }

  forceResume(): void {
    this.setState('ForcedActive');
  }

  pause(): void {
    this.setState('Paused');
  }

  deleteSelected(withData: boolean): void {
    const uuids = this.transfersStore.selectedUuids();
    if (uuids.length === 0) return;

    this.confirmation.confirm({
      header: 'Confirm Delete',
      message: withData
        ? `Delete ${uuids.length} ${uuids.length === 1 ? 'transfer' : 'transfers'} including downloaded data?`
        : `Delete ${uuids.length} ${uuids.length === 1 ? 'transfer' : 'transfers'}?`,
      accept: () => {
        this.transferApi.deleteTransfers(uuids, withData).subscribe({
          next: () => {
            this.transfersStore.clearSelection();
            this.transfersStore.refreshNow();
          },
          error: (err) => console.error('Failed to delete transfers', err),
        });
      },
    });
  }

  deleteCompleted(): void {
    const uuids = this.transfersStore
      .transfers()
      .filter((t) => t.state === 'Completed')
      .map((t) => t.uuid);
    if (uuids.length === 0) return;

    this.confirmation.confirm({
      header: 'Confirm Delete',
      message: `Delete ${uuids.length} completed ${uuids.length === 1 ? 'transfer' : 'transfers'}?`,
      accept: () => {
        this.transferApi.deleteTransfers(uuids, false).subscribe({
          next: () => this.transfersStore.refreshNow(),
          error: (err) => console.error('Failed to delete completed transfers', err),
        });
      },
    });
  }

  moveSelected(direction: 'top' | 'up' | 'down' | 'bottom'): void {
    const queueUuid = this.queuesStore.selectedQueueUuid();
    const uuids = this.transfersStore.selectedUuids();
    if (queueUuid === null || uuids.length === 0) return;

    this.transferApi.moveTransfers(queueUuid, uuids, direction).subscribe({
      next: () => this.transfersStore.refreshNow(),
      error: (err) => console.error('Failed to move transfers', err),
    });
  }

  private setState(state: Extract<TransferState, 'Waiting' | 'ForcedActive' | 'Paused'>): void {
    const uuids = this.transfersStore.selectedUuids();
    if (uuids.length === 0) return;

    this.transferApi.setTransferProperties(uuids, { state }).subscribe({
      next: () => this.transfersStore.refreshNow(),
      error: (err) => console.error('Failed to update transfer state', err),
    });
  }
}
