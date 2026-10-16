import { Injectable, inject, signal } from '@angular/core';
import { toObservable } from '@angular/core/rxjs-interop';
import { EMPTY, Subject, merge, of } from 'rxjs';
import { catchError } from 'rxjs/operators';
import { ConnectionHealth } from '../core/connection-health.service';
import { activeGatedPoll } from '../core/active-gated-poll';
import { TransferApi } from '../core/api/transfer-api';
import { PollingService } from '../core/polling.service';
import { QueuesStore } from './queues.store';
import { Transfer } from '../core/api/models';

/** Modifier keys held during a selection gesture. `ctrl` also covers Cmd on macOS. */
export interface SelectionModifiers {
  ctrl?: boolean;
  shift?: boolean;
}

/** Which selection-dependent actions apply right now; mirrors the legacy UI's transfersSelectionChanged(). */
export interface AvailableTransferActions {
  /** Some selected transfer isn't already Waiting/Active. */
  resume: boolean;
  forceResume: boolean;
  /** Some selected transfer isn't already Paused. */
  pause: boolean;
  delete: boolean;
  /** Some selected transfer isn't already first (also gates Move to Top). */
  moveUp: boolean;
  /** Some selected transfer isn't already last (also gates Move to Bottom). */
  moveDown: boolean;
  /**
   * Exactly one selected, as in the legacy UI: the dialog is prefilled from a single transfer,
   * so saving it for several would overwrite all their comments with that one.
   */
  properties: boolean;
}

export function availableTransferActions(transfers: Transfer[], selectedUuids: string[]): AvailableTransferActions {
  const selected = new Set(selectedUuids);
  const result: AvailableTransferActions = {
    resume: false,
    forceResume: selected.size > 0,
    pause: false,
    delete: selected.size > 0,
    moveUp: false,
    moveDown: false,
    properties: selected.size === 1,
  };
  transfers.forEach((t, index) => {
    if (!selected.has(t.uuid)) return;
    result.resume ||= t.state !== 'Waiting' && t.state !== 'Active';
    result.pause ||= t.state !== 'Paused';
    result.moveUp ||= index > 0;
    result.moveDown ||= index < transfers.length - 1;
  });
  return result;
}

@Injectable({ providedIn: 'root' })
export class TransfersStore {
  private readonly transferApi = inject(TransferApi);
  private readonly polling = inject(PollingService);
  private readonly queuesStore = inject(QueuesStore);
  private readonly health = inject(ConnectionHealth);

  readonly transfers = signal<Transfer[]>([]);
  readonly selectedUuids = signal<string[]>([]);
  /** The row keyboard navigation moves from, like QAbstractItemView's current index. */
  readonly currentUuid = signal<string | null>(null);

  /** Fixed end of a Shift range, like QItemSelectionModel's anchor. */
  private anchorUuid: string | null = null;

  private readonly refreshRequests$ = new Subject<number>();

  constructor() {
    // Fetches immediately whenever the selected queue changes (not just on the next tick), and the
    // switchMap layers cancel an in-flight fetch for a queue that is no longer selected, so a late
    // response can't overwrite the newly selected queue's transfers.
    activeGatedPoll(
      of(true),
      toObservable(this.queuesStore.selectedQueueUuid),
      merge(this.polling.ticks$, this.refreshRequests$),
      // A failed fetch keeps the last known transfers and must not terminate the poll stream.
      (queueUuid) =>
        queueUuid === null
          ? of(null)
          : this.health.track('transfers', this.transferApi.getQueueTransfers(queueUuid)).pipe(catchError(() => EMPTY)),
    ).subscribe(({ value }) => this.apply(value));
  }

  /** Triggers an immediate fetch, bypassing the poll interval — call after a mutating action so the UI updates right away. */
  refreshNow(): void {
    this.refreshRequests$.next(0);
  }

  isSelected(uuid: string): boolean {
    return this.selectedUuids().includes(uuid);
  }

  toggleSelection(uuid: string): void {
    const current = this.selectedUuids();
    this.selectedUuids.set(current.includes(uuid) ? current.filter((u) => u !== uuid) : [...current, uuid]);
    this.anchorUuid = uuid;
    this.currentUuid.set(uuid);
  }

  selectOnly(uuid: string): void {
    this.selectedUuids.set([uuid]);
    this.anchorUuid = uuid;
    this.currentUuid.set(uuid);
  }

  clearSelection(): void {
    this.selectedUuids.set([]);
  }

  /**
   * Click semantics of a Qt ExtendedSelection view: plain click selects only the row, Ctrl toggles
   * it, Shift selects the range from the anchor, Ctrl+Shift adds that range to the selection.
   */
  select(uuid: string, modifiers: SelectionModifiers = {}): void {
    if (modifiers.shift && this.anchorUuid !== null && this.indexOf(this.anchorUuid) >= 0) {
      const range = this.range(this.anchorUuid, uuid);
      if (modifiers.ctrl) {
        const merged = new Set([...this.selectedUuids(), ...range]);
        this.selectedUuids.set(this.inTableOrder(merged));
      } else {
        this.selectedUuids.set(range);
      }
      this.currentUuid.set(uuid);
    } else if (modifiers.ctrl) {
      this.toggleSelection(uuid);
    } else {
      this.selectOnly(uuid);
    }
  }

  selectAll(): void {
    this.selectedUuids.set(this.transfers().map((t) => t.uuid));
  }

  /** Keyboard navigation: moves the current row by `delta` (clamped), extending the range with Shift. */
  moveCurrent(delta: number, modifiers: SelectionModifiers = {}): void {
    const transfers = this.transfers();
    if (transfers.length === 0) return;
    const from = this.currentUuid() === null ? -1 : this.indexOf(this.currentUuid()!);
    const to = from < 0 ? (delta > 0 ? 0 : transfers.length - 1) : Math.max(0, Math.min(transfers.length - 1, from + delta));
    this.select(transfers[to].uuid, { shift: modifiers.shift });
  }

  private indexOf(uuid: string): number {
    return this.transfers().findIndex((t) => t.uuid === uuid);
  }

  private range(fromUuid: string, toUuid: string): string[] {
    const a = this.indexOf(fromUuid);
    const b = this.indexOf(toUuid);
    return this.transfers()
      .slice(Math.min(a, b), Math.max(a, b) + 1)
      .map((t) => t.uuid);
  }

  private inTableOrder(uuids: Set<string>): string[] {
    return this.transfers()
      .map((t) => t.uuid)
      .filter((u) => uuids.has(u));
  }

  /** `null` means no queue is selected. */
  private apply(transfers: Transfer[] | null): void {
    if (transfers === null) {
      this.transfers.set([]);
      this.selectedUuids.set([]);
      this.currentUuid.set(null);
      this.anchorUuid = null;
      return;
    }
    this.transfers.set(transfers);
    const validUuids = new Set(transfers.map((t) => t.uuid));
    this.selectedUuids.update((selected) => selected.filter((u) => validUuids.has(u)));
  }
}
