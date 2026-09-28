import { Injectable, inject } from '@angular/core';
import { ConfirmationService } from 'primeng/api';
import { TransferApi } from './api/transfer-api';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { TransferState, NewTransferOptions } from './api/models';

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

  updateProperties(comment: string, speedLimits: { down: number; up: number }): void {
    const uuids = this.transfersStore.selectedUuids();
    if (uuids.length === 0) return;

    // Backend requires userSpeedLimits as a 2-element [down, up] array, not a {down, up} struct.
    this.transferApi
      .setTransferProperties(uuids, { comment, userSpeedLimits: [speedLimits.down, speedLimits.up] })
      .subscribe({
        next: () => this.transfersStore.refreshNow(),
        error: (err) => console.error('Failed to update transfer properties', err),
      });
  }

  addTransfers(urls: string[], options: NewTransferOptions): void {
    if (urls.length === 0) return;

    this.transferApi.addTransfers(urls, options).subscribe({
      next: () => this.transfersStore.refreshNow(),
      error: (err) => console.error('Failed to add transfers', err),
    });
  }

  addTransferWithData(file: File, options: NewTransferOptions): void {
    this.transferApi.addTransferWithData(file, options).subscribe({
      next: () => this.transfersStore.refreshNow(),
      error: (err) => console.error('Failed to add transfer from file', err),
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
