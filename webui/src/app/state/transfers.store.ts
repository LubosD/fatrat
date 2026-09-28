import { Injectable, inject, signal } from '@angular/core';
import { startWith } from 'rxjs/operators';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';
import { QueuesStore } from './queues.store';
import { Transfer } from '../core/api/models';

@Injectable({ providedIn: 'root' })
export class TransfersStore {
  private readonly transferApi = inject(TransferApi);
  private readonly polling = inject(PollingService);
  private readonly queuesStore = inject(QueuesStore);

  readonly transfers = signal<Transfer[]>([]);
  readonly selectedUuids = signal<string[]>([]);

  constructor() {
    this.polling.ticks$.pipe(startWith(0)).subscribe(() => this.refresh());
  }

  /** Triggers an immediate fetch, bypassing the poll interval — call after a mutating action so the UI updates right away. */
  refreshNow(): void {
    this.refresh();
  }

  isSelected(uuid: string): boolean {
    return this.selectedUuids().includes(uuid);
  }

  toggleSelection(uuid: string): void {
    const current = this.selectedUuids();
    this.selectedUuids.set(current.includes(uuid) ? current.filter((u) => u !== uuid) : [...current, uuid]);
  }

  selectOnly(uuid: string): void {
    this.selectedUuids.set([uuid]);
  }

  clearSelection(): void {
    this.selectedUuids.set([]);
  }

  private refresh(): void {
    const uuid = this.queuesStore.selectedQueueUuid();
    if (uuid === null) {
      this.transfers.set([]);
      this.selectedUuids.set([]);
      return;
    }
    this.transferApi.getQueueTransfers(uuid).subscribe((transfers) => {
      this.transfers.set(transfers);
      const validUuids = new Set(transfers.map((t) => t.uuid));
      this.selectedUuids.update((selected) => selected.filter((u) => validUuids.has(u)));
    });
  }
}
