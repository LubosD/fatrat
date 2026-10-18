import { Injectable, inject } from '@angular/core';
import { ConfirmationService } from 'primeng/api';
import { Observable, of } from 'rxjs';
import { tap } from 'rxjs/operators';
import { TransferApi } from './api/transfer-api';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { JsonValue } from './api/json-value';
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

  /** Drag-and-drop reorder: moves the given transfers in front of `before` (null = end of queue). */
  moveBefore(uuids: string[], before: string | null): void {
    const queueUuid = this.queuesStore.selectedQueueUuid();
    if (queueUuid === null || uuids.length === 0) return;

    this.transferApi.moveTransfersBefore(queueUuid, uuids, before).subscribe({
      next: () => this.transfersStore.refreshNow(),
      error: (err) => console.error('Failed to move transfers', err),
    });
  }

  /**
   * PATCHes the selected transfers. The caller subscribes, so the Transfer Properties dialog can
   * stay open and show the server's message when the change is refused.
   */
  updateProperties(properties: { [key: string]: JsonValue }): Observable<void> {
    const uuids = this.transfersStore.selectedUuids();
    if (uuids.length === 0) return of(undefined);

    return this.transferApi.setTransferProperties(uuids, properties).pipe(tap(() => this.transfersStore.refreshNow()));
  }

  /** Reloads the transfer list now, e.g. after a change made through another endpoint. */
  refresh(): void {
    this.transfersStore.refreshNow();
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
