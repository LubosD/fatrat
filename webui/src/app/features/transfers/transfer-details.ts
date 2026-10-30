import {
  Component,
  computed,
  effect,
  inject,
  signal,
  ChangeDetectionStrategy,
} from '@angular/core';
import { NgComponentOutlet } from '@angular/common';
import { Tree } from 'primeng/tree';
import type { TreeNode } from 'primeng/api';
import type { TreeNodeExpandEvent, TreeNodeSelectEvent } from 'primeng/tree';
import { TransfersStore } from '../../state/transfers.store';
import { BrowseService } from '../../core/browse.service';
import { TRANSFER_DETAIL_COMPONENTS } from './transfer-detail-registry';
import { elideMiddle } from './format';

const DETAILS_NAME_MAX_LENGTH = 80;

@Component({
  selector: 'app-transfer-details',
  imports: [Tree, NgComponentOutlet],
  changeDetection: ChangeDetectionStrategy.Eager,
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

  protected readonly elideName = (name: string): string =>
    elideMiddle(name, DETAILS_NAME_MAX_LENGTH);

  protected readonly treeNodes = signal<TreeNode[]>([]);

  protected readonly detailComponent = computed(() => {
    const script = this.selectedTransfer()?.detailsScript;
    return script ? (TRANSFER_DETAIL_COMPONENTS[script] ?? null) : null;
  });

  /**
   * The uuid of the transfer we last fetched (or cleared) the tree for. A plain instance field
   * rather than a signal: TransfersStore.refresh() replaces `transfers` with a brand-new array of
   * brand-new Transfer objects on every poll tick, so selectedTransfer() produces a new object
   * reference every tick even when the selection hasn't changed. Angular's effect() reruns
   * whenever any signal it reads changes by reference, so without this manual guard the effect
   * below would refetch on every poll tick instead of only when the selection actually changes.
   */
  private lastFetchedUuid: string | null = null;

  constructor() {
    effect(() => {
      const transfer = this.selectedTransfer();
      const uuid = transfer?.uuid ?? null;
      if (uuid === this.lastFetchedUuid) {
        return;
      }
      this.lastFetchedUuid = uuid;
      if (!transfer?.dataPathIsDir) {
        this.treeNodes.set([]);
        return;
      }
      const transferUuid = transfer.uuid;
      this.browse.list(transferUuid, '').subscribe({
        next: (nodes) => {
          // Guard against a stale response: only apply it if this is still the selected transfer.
          if (this.selectedTransfer()?.uuid === transferUuid) {
            this.treeNodes.set(nodes);
          }
        },
        error: (err) => console.error('Failed to list transfer directory', err),
      });
    });
  }

  protected downloadUrl(transferUuid: string, path?: string): string {
    const base = `/download?transfer=${encodeURIComponent(transferUuid)}`;
    return path === undefined ? base : `${base}&path=${encodeURIComponent(path)}`;
  }

  protected onNodeExpand(event: TreeNodeExpandEvent): void {
    const transfer = this.selectedTransfer();
    const node = event.node;
    if (!transfer || node.leaf || node.children) {
      return;
    }
    const transferUuid = transfer.uuid;
    this.browse.list(transferUuid, node.key ?? '').subscribe({
      next: (children) => {
        // Guard against a stale response: only apply it if this is still the selected transfer.
        if (this.selectedTransfer()?.uuid !== transferUuid) {
          return;
        }
        node.children = children;
        this.treeNodes.set([...this.treeNodes()]);
      },
      error: (err) => console.error('Failed to list transfer directory', err),
    });
  }

  protected onNodeSelect(event: TreeNodeSelectEvent): void {
    const transfer = this.selectedTransfer();
    if (!transfer || !event.node.leaf) {
      return;
    }
    this.navigateTo(this.downloadUrl(transfer.uuid, event.node.key));
  }

  /** Extracted so tests can spy on it instead of stubbing window.location (fragile under happy-dom). */
  private navigateTo(url: string): void {
    window.location.href = url;
  }
}
