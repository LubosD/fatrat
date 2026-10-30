import { Component, inject, output, ChangeDetectionStrategy } from '@angular/core';
import type { MenuItem } from '@openng/optimus-ui/api';
import { ContextMenu } from '@openng/optimus-ui/contextmenu';
import { QueuesStore } from '../../state/queues.store';
import { QueueActionsService } from '../../core/queue-actions.service';
import { Queue } from '../../core/api/models';
import { watchStatusProblem } from './watch-status';

@Component({
  selector: 'app-queue-list',
  imports: [ContextMenu],
  templateUrl: './queue-list.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './queue-list.scss',
})
export class QueueList {
  protected readonly store = inject(QueuesStore);
  private readonly queueActions = inject(QueueActionsService);

  readonly propertiesRequested = output<void>();
  // The delete dialog and confirmation flow live at the shell level, like Queue Properties,
  // so the toolbar/overflow-menu "Delete Queue" action works without this component being mounted.
  readonly deleteRequested = output<void>();

  // Same entries as the desktop client's queue menu.
  protected readonly contextMenuItems: MenuItem[] = [
    {
      label: 'Resume All',
      icon: 'pi pi-play',
      command: () => this.withSelectedQueue((uuid) => this.queueActions.resumeAll(uuid)),
    },
    {
      label: 'Stop All',
      icon: 'pi pi-pause',
      command: () => this.withSelectedQueue((uuid) => this.queueActions.stopAll(uuid)),
    },
    { separator: true },
    {
      label: 'Properties',
      icon: 'pi pi-sliders-h',
      command: () => this.propertiesRequested.emit(),
    },
    { separator: true },
    { label: 'Delete Queue', icon: 'pi pi-trash', command: () => this.deleteRequested.emit() },
  ];

  protected watchProblem(queue: Queue): string | null {
    return watchStatusProblem(queue.watchStatus);
  }

  protected onContextMenu(event: MouseEvent, uuid: string, menu: ContextMenu): void {
    // The menu acts on the selected queue, so a right-click selects first.
    this.store.selectQueue(uuid);
    menu.show(event);
    event.preventDefault();
  }

  private withSelectedQueue(fn: (uuid: string) => void): void {
    const uuid = this.store.selectedQueueUuid();
    if (uuid !== null) fn(uuid);
  }
}
