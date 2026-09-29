import { Component, computed, inject } from '@angular/core';
import { TransfersStore } from '../../state/transfers.store';
import { BrowseService } from '../../core/browse.service';

@Component({
  selector: 'app-transfer-details',
  templateUrl: './transfer-details.html',
})
export class TransferDetails {
  private readonly store = inject(TransfersStore);
  private readonly browse = inject(BrowseService);

  protected readonly selectedTransfer = computed(() => {
    const uuids = this.store.selectedUuids();
    if (uuids.length !== 1) {
      return null;
    }
    return this.store.transfers().find((t) => t.uuid === uuids[0]) ?? null;
  });

  protected downloadUrl(transferUuid: string, path?: string): string {
    const base = `/download?transfer=${encodeURIComponent(transferUuid)}`;
    return path === undefined ? base : `${base}&path=${encodeURIComponent(path)}`;
  }
}
