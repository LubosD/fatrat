import { Component, inject, output, signal } from '@angular/core';
import type { MenuItem } from 'primeng/api';
import { ContextMenu } from 'primeng/contextmenu';
import { DeleteQueueDialog } from './delete-queue-dialog';
import { DeleteQueueOptions, Queue } from '../../core/api/models';
import { QueuesStore } from '../../state/queues.store';
import { QueueActionsService } from '../../core/queue-actions.service';

@Component({
  selector: 'app-queue-list',
  imports: [ContextMenu, DeleteQueueDialog],
  templateUrl: './queue-list.html',
  styleUrl: './queue-list.scss',
})
export class QueueList {
  protected readonly store = inject(QueuesStore);
  private readonly queueActions = inject(QueueActionsService);

  protected readonly deleteTarget = signal<Queue | null>(null);
  protected readonly deleteTransferCount = signal(0);
  protected readonly showDeleteDialog = signal(false);

  readonly propertiesRequested = output<void>();

  // Same entries as the desktop client's queue menu.
  protected readonly contextMenuItems: MenuItem[] = [
    { label: 'Resume All', icon: 'pi pi-play', command: () => this.withSelectedQueue((uuid) => this.queueActions.resumeAll(uuid)) },
    { label: 'Stop All', icon: 'pi pi-pause', command: () => this.withSelectedQueue((uuid) => this.queueActions.stopAll(uuid)) },
    { separator: true },
    { label: 'Properties', icon: 'pi pi-sliders-h', command: () => this.propertiesRequested.emit() },
    { separator: true },
    { label: 'Delete Queue', icon: 'pi pi-trash', command: () => this.deleteSelectedQueue() },
  ];

  protected onContextMenu(event: MouseEvent, uuid: string, menu: ContextMenu): void {
    // The menu acts on the selected queue, so a right-click selects first.
    this.store.selectQueue(uuid);
    menu.show(event);
    event.preventDefault();
  }

  protected otherQueues(): Queue[] {
    return this.store.queues().filter((q) => q.uuid !== this.deleteTarget()?.uuid);
  }

  protected onDeleteConfirmed(options: DeleteQueueOptions): void {
    const queue = this.deleteTarget();
    if (queue) this.queueActions.deleteQueue(queue, options);
  }

  private deleteSelectedQueue(): void {
    const uuid = this.store.selectedQueueUuid();
    const queue = this.store.queues().find((q) => q.uuid === uuid);
    if (!queue) return;

    this.queueActions.countTransfers(queue.uuid).subscribe({
      next: (count) => {
        if (count === 0) {
          this.queueActions.confirmDeleteEmptyQueue(queue);
        } else {
          this.deleteTarget.set(queue);
          this.deleteTransferCount.set(count);
          this.showDeleteDialog.set(true);
        }
      },
      error: (err) => console.error('Failed to count queue transfers', err),
    });
  }

  private withSelectedQueue(fn: (uuid: string) => void): void {
    const uuid = this.store.selectedQueueUuid();
    if (uuid !== null) fn(uuid);
  }
}
