import { Component, inject } from '@angular/core';
import { TableModule } from 'primeng/table';
import type { TableContextMenuSelectEvent } from 'primeng/table';
import { ProgressBar } from 'primeng/progressbar';
import { ContextMenu } from 'primeng/contextmenu';
import type { MenuItem } from 'primeng/api';
import { TransfersStore } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer } from '../../core/api/models';
import { formatEta, formatSize, formatSpeed } from './format';

@Component({
  selector: 'app-transfer-table',
  imports: [TableModule, ProgressBar, ContextMenu],
  templateUrl: './transfer-table.html',
})
export class TransferTable {
  protected readonly store = inject(TransfersStore);
  private readonly actions = inject(TransferActionsService);

  protected readonly formatSize = formatSize;
  protected readonly formatSpeed = formatSpeed;
  protected readonly formatEta = formatEta;

  protected readonly trackByUuid = (_: number, transfer: Transfer): string => transfer.uuid;

  protected readonly contextMenuItems: MenuItem[] = [
    { label: 'Resume', command: () => this.actions.resume() },
    { label: 'Force Resume', command: () => this.actions.forceResume() },
    { label: 'Pause', command: () => this.actions.pause() },
    { label: 'Delete', command: () => this.actions.deleteSelected(false) },
    { label: 'Delete with Data', command: () => this.actions.deleteSelected(true) },
    { label: 'Move to Top', command: () => this.actions.moveSelected('top') },
    { label: 'Move Up', command: () => this.actions.moveSelected('up') },
    { label: 'Move Down', command: () => this.actions.moveSelected('down') },
    { label: 'Move to Bottom', command: () => this.actions.moveSelected('bottom') },
  ];

  protected progress(total: number, done: number): number {
    return total > 0 ? Math.min(100, Math.round((done / total) * 100)) : 0;
  }

  protected onContextMenuSelect(event: TableContextMenuSelectEvent<Transfer>): void {
    if (!this.store.isSelected(event.data.uuid)) {
      this.store.selectOnly(event.data.uuid);
    }
  }
}
