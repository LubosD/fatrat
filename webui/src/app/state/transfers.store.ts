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

  constructor() {
    this.polling.ticks$.pipe(startWith(0)).subscribe(() => this.refresh());
  }

  private refresh(): void {
    const uuid = this.queuesStore.selectedQueueUuid();
    if (uuid === null) {
      this.transfers.set([]);
      return;
    }
    this.transferApi.getQueueTransfers(uuid).subscribe((transfers) => this.transfers.set(transfers));
  }
}
