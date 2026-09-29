import { Component, computed, effect, inject, signal } from '@angular/core';
import { Tree } from 'primeng/tree';
import type { TreeNode } from 'primeng/api';
import type { TreeNodeExpandEvent, TreeNodeSelectEvent } from 'primeng/tree';
import { TransfersStore } from '../../state/transfers.store';
import { BrowseService } from '../../core/browse.service';

@Component({
  selector: 'app-transfer-details',
  imports: [Tree],
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

  protected readonly treeNodes = signal<TreeNode[]>([]);

  constructor() {
    effect(() => {
      const transfer = this.selectedTransfer();
      if (!transfer?.dataPathIsDir) {
        this.treeNodes.set([]);
        return;
      }
      this.browse.list(transfer.uuid, '').subscribe({
        next: (nodes) => this.treeNodes.set(nodes),
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
    this.browse.list(transfer.uuid, node.key ?? '').subscribe({
      next: (children) => {
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
