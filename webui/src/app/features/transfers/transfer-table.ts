import { Component, ElementRef, EventEmitter, Output, inject } from '@angular/core';
import { TableModule } from 'primeng/table';
import type { TableContextMenuSelectEvent } from 'primeng/table';
import { ProgressBar } from 'primeng/progressbar';
import { ContextMenu } from 'primeng/contextmenu';
import type { MenuItem } from 'primeng/api';
import { AvailableTransferActions, TransfersStore, availableTransferActions } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer, TransferState } from '../../core/api/models';
import { formatEta, formatSize, formatSpeed } from './format';

/** Icon file stems under img/states/, ported from the legacy web UI. Ready/Testing have none. */
const STATE_ICONS: Partial<Record<TransferState, string>> = {
  Waiting: 'waiting',
  Active: 'active',
  ForcedActive: 'forcedactive',
  Paused: 'paused',
  Failed: 'failed',
  Completed: 'completed',
};

const STATE_LABELS: Partial<Record<TransferState, string>> = {
  ForcedActive: 'Forced active',
};

/** Same rule as the legacy UI: a seeding/uploading transfer gets the upload variant. */
function isUploadIcon(transfer: Transfer): boolean {
  return transfer.mode === 'Upload' && (transfer.primaryMode === 'Upload' || transfer.state !== 'Completed');
}

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

  // Icons match the toolbar's; grouping and Properties follow the legacy web UI's menu. Each `id`
  // is the AvailableTransferActions key that gates the item.
  protected contextMenuItems: MenuItem[] = [
    { id: 'resume', label: 'Resume', icon: 'pi pi-play', command: () => this.actions.resume() },
    { id: 'forceResume', label: 'Force Resume', icon: 'pi pi-forward', command: () => this.actions.forceResume() },
    { id: 'pause', label: 'Pause', icon: 'pi pi-pause', command: () => this.actions.pause() },
    { separator: true },
    { id: 'delete', label: 'Delete', icon: 'pi pi-trash', command: () => this.actions.deleteSelected(false) },
    { id: 'delete', label: 'Delete with Data', icon: 'pi pi-eraser', command: () => this.actions.deleteSelected(true) },
    { separator: true },
    { id: 'moveUp', label: 'Move to Top', icon: 'pi pi-angle-double-up', command: () => this.actions.moveSelected('top') },
    { id: 'moveUp', label: 'Move Up', icon: 'pi pi-angle-up', command: () => this.actions.moveSelected('up') },
    { id: 'moveDown', label: 'Move Down', icon: 'pi pi-angle-down', command: () => this.actions.moveSelected('down') },
    { id: 'moveDown', label: 'Move to Bottom', icon: 'pi pi-angle-double-down', command: () => this.actions.moveSelected('bottom') },
    { separator: true },
    { id: 'properties', label: 'Properties', icon: 'pi pi-pencil', command: () => this.propertiesRequested.emit() },
  ];

  protected progress(total: number, done: number): number {
    return total > 0 ? Math.min(100, Math.round((done / total) * 100)) : 0;
  }

  protected stateIcon(transfer: Transfer): string {
    return `img/states/${STATE_ICONS[transfer.state] ?? 'waiting'}${isUploadIcon(transfer) ? '_upload' : ''}.png`;
  }

  protected stateLabel(transfer: Transfer): string {
    return STATE_LABELS[transfer.state] ?? transfer.state;
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
    const available = availableTransferActions(this.store.transfers(), this.store.selectedUuids());
    this.contextMenuItems = this.contextMenuItems.map((item) =>
      item.id ? { ...item, disabled: !available[item.id as keyof AvailableTransferActions] } : item,
    );
  }
}
