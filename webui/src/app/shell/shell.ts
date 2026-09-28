import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Toolbar } from 'primeng/toolbar';
import { Select } from 'primeng/select';
import { Splitter } from 'primeng/splitter';
import { ButtonDirective } from 'primeng/button';
import { ConfirmDialog } from 'primeng/confirmdialog';
import { QueueList } from '../features/queues/queue-list';
import { TransferTable } from '../features/transfers/transfer-table';
import { QueuePropertiesDialog, QueueFormValue } from '../features/queues/queue-properties-dialog';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';
import { QueueActionsService } from '../core/queue-actions.service';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { Queue } from '../core/api/models';

@Component({
  selector: 'app-shell',
  imports: [FormsModule, Toolbar, Select, Splitter, ButtonDirective, ConfirmDialog, QueueList, TransferTable, QueuePropertiesDialog],
  templateUrl: './shell.html',
  styleUrl: './shell.scss',
})
export class Shell {
  protected readonly polling = inject(PollingService);
  protected readonly allowedIntervals: number[] = [...this.polling.allowedIntervals];
  protected readonly actions = inject(TransferActionsService);
  private readonly transfersStore = inject(TransfersStore);
  private readonly queueActions = inject(QueueActionsService);
  private readonly queuesStore = inject(QueuesStore);

  protected get hasSelection(): boolean {
    return this.transfersStore.selectedUuids().length > 0;
  }

  protected get hasSelectedQueue(): boolean {
    return this.queuesStore.selectedQueueUuid() !== null;
  }

  protected readonly showQueueDialog = signal(false);
  protected readonly queueDialogTarget = signal<Queue | null>(null);

  protected openNewQueueDialog(): void {
    this.queueDialogTarget.set(null);
    this.showQueueDialog.set(true);
  }

  protected openQueuePropertiesDialog(): void {
    const uuid = this.queuesStore.selectedQueueUuid();
    const queue = uuid === null ? null : (this.queuesStore.queues().find((q) => q.uuid === uuid) ?? null);
    this.queueDialogTarget.set(queue);
    this.showQueueDialog.set(true);
  }

  protected onQueueDialogSave(value: QueueFormValue): void {
    const target = this.queueDialogTarget();
    if (target === null) {
      this.queueActions.createQueue(value);
    } else {
      this.queueActions.updateQueueProperties(target.uuid, value);
    }
  }
}
