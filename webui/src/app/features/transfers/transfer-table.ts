import { Component, ElementRef, EventEmitter, Output, inject } from '@angular/core';
import { TableModule } from 'primeng/table';
import type { TableContextMenuSelectEvent } from 'primeng/table';
import { ProgressBar } from 'primeng/progressbar';
import { ContextMenu } from 'primeng/contextmenu';
import type { MenuItem } from 'primeng/api';
import { TransfersStore, availableTransferActions } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer } from '../../core/api/models';
import { progressPercent, stateIconPath, stateLabelText } from './transfer-presentation';
import { createTransferMenuItems, withAvailability } from './transfer-menu';
import { formatEta, formatSize, formatSpeed } from './format';

@Component({
  selector: 'app-transfer-table',
  imports: [TableModule, ProgressBar, ContextMenu],
  templateUrl: './transfer-table.html',
  styleUrl: './transfer-table.scss',
})
export class TransferTable {
  protected readonly store = inject(TransfersStore);
  private readonly actions = inject(TransferActionsService);
  private readonly host = inject<ElementRef<HTMLElement>>(ElementRef);

  protected readonly formatSize = formatSize;
  protected readonly formatSpeed = formatSpeed;
  protected readonly formatEta = formatEta;

  protected readonly trackByUuid = (_: number, transfer: Transfer): string => transfer.uuid;

  /** Emitted from the context menu; Shell owns the properties dialog. */
  @Output() readonly propertiesRequested = new EventEmitter<void>();

  protected contextMenuItems: MenuItem[] = createTransferMenuItems(this.actions, () => this.propertiesRequested.emit());

  protected progress(total: number, done: number): number {
    return progressPercent(total, done);
  }

  protected stateIcon(transfer: Transfer): string {
    return stateIconPath(transfer);
  }

  protected stateLabel(transfer: Transfer): string {
    return stateLabelText(transfer);
  }

  protected onRowClick(event: MouseEvent, uuid: string): void {
    this.store.select(uuid, { ctrl: event.ctrlKey || event.metaKey, shift: event.shiftKey });
  }

  protected onKeydown(event: KeyboardEvent): void {
    const ctrl = event.ctrlKey || event.metaKey;
    const shift = event.shiftKey;
    const page = 10;
    switch (event.key) {
      case 'ArrowUp':
        this.store.moveCurrent(-1, { shift });
        break;
      case 'ArrowDown':
        this.store.moveCurrent(1, { shift });
        break;
      case 'PageUp':
        this.store.moveCurrent(-page, { shift });
        break;
      case 'PageDown':
        this.store.moveCurrent(page, { shift });
        break;
      case 'Home':
        this.store.moveCurrent(-Infinity, { shift });
        break;
      case 'End':
        this.store.moveCurrent(Infinity, { shift });
        break;
      case 'a':
      case 'A':
        if (!ctrl) return;
        this.store.selectAll();
        break;
      default:
        return;
    }
    event.preventDefault();
    this.scrollCurrentIntoView();
  }

  /** Separate method so tests can spy on it; happy-dom has no layout engine. */
  protected scrollCurrentIntoView(): void {
    const uuid = this.store.currentUuid();
    if (uuid === null) return;
    const row = this.host.nativeElement.querySelector<HTMLElement>(`tr[data-uuid="${CSS.escape(uuid)}"]`);
    row?.scrollIntoView?.({ block: 'nearest' });
  }

  protected onContextMenuSelect(event: TableContextMenuSelectEvent<Transfer>): void {
    if (!this.store.isSelected(event.data.uuid)) {
      this.store.selectOnly(event.data.uuid);
    }
    // A fresh array, so the menu re-renders the new disabled flags.
    this.contextMenuItems = withAvailability(
      this.contextMenuItems,
      availableTransferActions(this.store.transfers(), this.store.selectedUuids()),
    );
  }
}
