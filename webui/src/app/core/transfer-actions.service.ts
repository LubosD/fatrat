import { Injectable, inject } from '@angular/core';
import { ConfirmationService } from 'primeng/api';
import { TransferApi } from './api/transfer-api';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';

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
        ? `Delete ${uuids.length} selected transfer(s) including downloaded data?`
        : `Delete ${uuids.length} selected transfer(s)?`,
      accept: () => {
        this.transferApi.deleteTransfers(uuids, withData).subscribe(() => {
          this.transfersStore.clearSelection();
          this.transfersStore.refreshNow();
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
      message: `Delete ${uuids.length} completed transfer(s)?`,
      accept: () => {
        this.transferApi.deleteTransfers(uuids, false).subscribe(() => this.transfersStore.refreshNow());
      },
    });
  }

  moveSelected(direction: 'top' | 'up' | 'down' | 'bottom'): void {
    const queueUuid = this.queuesStore.selectedQueueUuid();
    const uuids = this.transfersStore.selectedUuids();
    if (queueUuid === null || uuids.length === 0) return;

    this.transferApi.moveTransfers(queueUuid, uuids, direction).subscribe(() => this.transfersStore.refreshNow());
  }

  private setState(state: 'Waiting' | 'ForcedActive' | 'Paused'): void {
    const uuids = this.transfersStore.selectedUuids();
    if (uuids.length === 0) return;

    this.transferApi.setTransferProperties(uuids, { state }).subscribe(() => this.transfersStore.refreshNow());
  }
}
