import {
  Component,
  EventEmitter,
  Output,
  computed,
  inject,
  signal,
  ChangeDetectionStrategy,
} from '@angular/core';
import { Subscription } from 'rxjs';
import { ProgressBar } from '@openng/optimus-ui/progressbar';
import { Menu } from '@openng/optimus-ui/menu';
import { ButtonDirective, ButtonIcon } from '@openng/optimus-ui/button';
import type { MenuItem } from '@openng/optimus-ui/api';
import { TransfersStore, availableTransferActions } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer } from '../../core/api/models';
import { elideMiddle, formatEta, formatSize, formatSpeed } from './format';
import { progressPercent, stateIconPath, stateLabelText } from './transfer-presentation';
import { createTransferMenuItems, withAvailability } from './transfer-menu';
import { PluginActions } from './plugin-actions';

export const LONG_PRESS_MS = 500;
/** A finger that drifts further than this is scrolling, not pressing. */
const LONG_PRESS_MOVE_TOLERANCE_PX = 10;
/** Cards are narrower than the table, so names get less room before being elided. */
const CARD_NAME_MAX_LENGTH = 60;

/**
 * Touch-friendly alternative to TransferTable: one card per transfer. Tap selects a card; a long
 * press enters selection mode, where taps toggle cards. Selection lives in TransfersStore, so the
 * top bar's actions work on it exactly as they do for the table.
 */
@Component({
  selector: 'app-transfer-cards',
  imports: [ProgressBar, Menu, ButtonDirective, ButtonIcon],
  templateUrl: './transfer-cards.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './transfer-cards.scss',
})
export class TransferCards {
  protected readonly store = inject(TransfersStore);
  private readonly actions = inject(TransferActionsService);
  private readonly pluginActions = inject(PluginActions);
  private pluginMenu?: Subscription;

  /** Emitted from the card menu; Shell owns the properties dialog. */
  @Output() readonly propertiesRequested = new EventEmitter<void>();

  protected readonly selectionMode = signal(false);
  protected readonly selectedCount = computed(() => this.store.selectedUuids().length);
  protected readonly menuItems = signal<MenuItem[]>([]);

  protected readonly formatSize = formatSize;
  protected readonly formatSpeed = formatSpeed;
  protected readonly formatEta = formatEta;
  protected readonly elideName = (name: string): string => elideMiddle(name, CARD_NAME_MAX_LENGTH);
  protected readonly progress = progressPercent;
  protected readonly stateIcon = stateIconPath;
  protected readonly stateLabel = stateLabelText;
  protected readonly trackByUuid = (_: number, transfer: Transfer): string => transfer.uuid;

  private readonly baseMenuItems = createTransferMenuItems(this.actions, () =>
    this.propertiesRequested.emit(),
  );

  private pressTimer: ReturnType<typeof setTimeout> | null = null;
  private pressOrigin: { x: number; y: number } | null = null;
  /** Set when a long press fired, so the click that follows the finger lifting is ignored. */
  private suppressNextClick = false;

  protected onPointerDown(event: PointerEvent, uuid: string): void {
    this.cancelPress();
    // A long press whose lift produced no click must not swallow this new gesture's tap.
    this.suppressNextClick = false;
    this.pressOrigin = { x: event.clientX, y: event.clientY };
    this.pressTimer = setTimeout(() => {
      this.pressTimer = null;
      this.suppressNextClick = true;
      this.selectionMode.set(true);
      this.store.toggleSelection(uuid);
    }, LONG_PRESS_MS);
  }

  protected onPointerMove(event: PointerEvent): void {
    if (this.pressTimer === null || this.pressOrigin === null) return;
    if (
      Math.hypot(event.clientX - this.pressOrigin.x, event.clientY - this.pressOrigin.y) >
      LONG_PRESS_MOVE_TOLERANCE_PX
    ) {
      this.cancelPress();
    }
  }

  protected cancelPress(): void {
    if (this.pressTimer !== null) {
      clearTimeout(this.pressTimer);
      this.pressTimer = null;
    }
    this.pressOrigin = null;
  }

  protected onCardClick(uuid: string): void {
    if (this.suppressNextClick) {
      this.suppressNextClick = false;
      return;
    }
    if (!this.selectionMode()) {
      this.store.selectOnly(uuid);
      return;
    }
    this.store.toggleSelection(uuid);
    if (this.store.selectedUuids().length === 0) {
      this.selectionMode.set(false);
    }
  }

  protected exitSelectionMode(): void {
    this.selectionMode.set(false);
    this.store.clearSelection();
  }

  protected selectAll(): void {
    this.store.selectAll();
  }

  protected openMenu(event: Event, uuid: string, menu: Menu): void {
    event.stopPropagation();
    if (!this.store.isSelected(uuid)) {
      this.store.selectOnly(uuid);
    }
    this.menuItems.set(
      withAvailability(
        this.baseMenuItems,
        availableTransferActions(this.store.transfers(), this.store.selectedUuids()),
      ),
    );
    const base = this.menuItems();
    this.pluginMenu?.unsubscribe();
    const selected = this.store.selectedUuids();
    if (selected.length === 1) {
      this.pluginMenu = this.pluginActions
        .menuItems(selected[0])
        .subscribe((extra) => this.menuItems.set([...base, ...extra]));
    }
    menu.toggle(event);
  }
}
