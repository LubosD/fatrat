import { ChangeDetectorRef, Component, ElementRef, EventEmitter, Output, inject, signal } from '@angular/core';
import { Subscription } from 'rxjs';
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
import { PluginActions } from './plugin-actions';
import { elideMiddle, formatEta, formatSize, formatSpeed } from './format';

/** Magnet links without metadata yet carry hash-long names; elide them instead of letting the column stretch. */
const TABLE_NAME_MAX_LENGTH = 80;

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
  protected readonly elideName = (name: string): string => elideMiddle(name, TABLE_NAME_MAX_LENGTH);

  protected readonly trackByUuid = (_: number, transfer: Transfer): string => transfer.uuid;

  /** Emitted from the context menu; Shell owns the properties dialog. */
  @Output() readonly propertiesRequested = new EventEmitter<void>();

  private readonly pluginActions = inject(PluginActions);
  private readonly cdr = inject(ChangeDetectorRef);
  private readonly baseMenuItems: MenuItem[] = createTransferMenuItems(this.actions, () => this.propertiesRequested.emit());
  private pluginMenu?: Subscription;
  protected contextMenuItems: MenuItem[] = this.baseMenuItems;

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

  /** Uuids being dragged (the whole selection when the grabbed row is part of it). */
  private dragUuids: string[] = [];
  protected readonly dragging = signal<ReadonlySet<string>>(new Set());
  protected readonly dropTarget = signal<{ uuid: string; after: boolean } | null>(null);

  protected isDragged(uuid: string): boolean {
    return this.dragging().has(uuid);
  }

  protected onDragStart(event: DragEvent, uuid: string): void {
    if (!this.store.isSelected(uuid)) {
      this.store.selectOnly(uuid);
    }
    this.dragUuids = this.store.selectedUuids();
    this.dragging.set(new Set(this.dragUuids));
    if (event.dataTransfer) {
      event.dataTransfer.effectAllowed = 'move';
      // Firefox only starts a drag when some data is set.
      event.dataTransfer.setData('text/plain', this.dragUuids.join('\n'));
    }
  }

  protected onDragOver(event: DragEvent, uuid: string): void {
    if (this.dragUuids.length === 0) return;
    event.preventDefault();
    if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
    const row = event.currentTarget as HTMLElement;
    const rect = row.getBoundingClientRect();
    const after = event.clientY > rect.top + rect.height / 2;
    const current = this.dropTarget();
    if (current?.uuid !== uuid || current.after !== after) {
      this.dropTarget.set({ uuid, after });
    }
  }

  protected onDrop(event: DragEvent, uuid: string): void {
    if (this.dragUuids.length === 0) return;
    event.preventDefault();
    const target = this.dropTarget();
    const after = target?.uuid === uuid ? target.after : false;
    const uuids = this.dragUuids;
    this.onDragEnd();

    const transfers = this.store.transfers();
    const index = transfers.findIndex((t) => t.uuid === uuid);
    if (index < 0) return;
    // Insertion point, then the first row at or after it that isn't itself being moved.
    const moved = new Set(uuids);
    const before = transfers.slice(after ? index + 1 : index).find((t) => !moved.has(t.uuid))?.uuid ?? null;
    this.actions.moveBefore(uuids, before);
  }

  protected onDragEnd(): void {
    this.dragUuids = [];
    this.dragging.set(new Set());
    this.dropTarget.set(null);
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
    const base = withAvailability(this.baseMenuItems, availableTransferActions(this.store.transfers(), this.store.selectedUuids()));
    this.contextMenuItems = base;

    this.pluginMenu?.unsubscribe();
    const selected = this.store.selectedUuids();
    if (selected.length === 1) {
      this.pluginMenu = this.pluginActions.menuItems(selected[0]).subscribe((extra) => {
        this.contextMenuItems = [...base, ...extra];
        this.cdr.markForCheck();
      });
    }
  }
}
