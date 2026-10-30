import { Injectable, inject } from '@angular/core';
import { ConfirmationService } from '@openng/optimus-ui/api';
import { Observable, of } from 'rxjs';
import { map, tap } from 'rxjs/operators';
import { TransferApi } from './api/transfer-api';
import { TransfersStore, resumedState } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { JsonValue } from './api/json-value';
import { TransferState, NewTransferOptions } from './api/models';

@Injectable({ providedIn: 'root' })
export class TransferActionsService {
  private readonly transferApi = inject(TransferApi);
  private readonly transfersStore = inject(TransfersStore);
  private readonly queuesStore = inject(QueuesStore);
  private readonly confirmation = inject(ConfirmationService);

  /** Per transfer, like the desktop: forced -> Active, paused/failed -> Waiting; the rest is left alone. */
  resume(): void {
    const selected = new Set(this.transfersStore.selectedUuids());
    const active: string[] = [];
    const waiting: string[] = [];
    for (const t of this.transfersStore.transfers()) {
      if (!selected.has(t.uuid)) continue;
      const next = resumedState(t.state);
      if (next === 'Active') active.push(t.uuid);
      else if (next === 'Waiting') waiting.push(t.uuid);
    }
    if (active.length > 0) this.setStateOf(active, 'Active');
    if (waiting.length > 0) this.setStateOf(waiting, 'Waiting');
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

  /**
   * Adds the URLs; the caller (the New Transfer dialog) shows a failure and keeps the user's input,
   * so errors are not swallowed here. Completes without a request when there is nothing to add.
   */
  addTransfers(urls: string[], options: NewTransferOptions): Observable<void> {
    if (urls.length === 0) return of(undefined);

    return this.transferApi.addTransfers(urls, options).pipe(
      tap(() => this.transfersStore.refreshNow()),
      map(() => undefined),
    );
  }

  addTransferWithData(file: File, options: NewTransferOptions): Observable<void> {
    return this.transferApi.addTransferWithData(file, options).pipe(
      tap(() => this.transfersStore.refreshNow()),
      map(() => undefined),
    );
  }

  private setState(state: Extract<TransferState, 'Waiting' | 'ForcedActive' | 'Paused'>): void {
    const uuids = this.transfersStore.selectedUuids();
    if (uuids.length === 0) return;
    this.setStateOf(uuids, state);
  }

  private setStateOf(uuids: string[], state: Extract<TransferState, 'Active' | 'Waiting' | 'ForcedActive' | 'Paused'>): void {
    this.transferApi.setTransferProperties(uuids, { state }).subscribe({
      next: () => this.transfersStore.refreshNow(),
      error: (err) => console.error('Failed to update transfer state', err),
    });
  }
}
