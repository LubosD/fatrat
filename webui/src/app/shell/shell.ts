import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Toolbar } from 'primeng/toolbar';
import { Select } from 'primeng/select';
import { Splitter } from 'primeng/splitter';
import { ButtonDirective, ButtonIcon } from 'primeng/button';
import { Tooltip } from 'primeng/tooltip';
import { ConfirmDialog } from 'primeng/confirmdialog';
import { Tabs, TabList, Tab, TabPanels, TabPanel } from 'primeng/tabs';
import { QueueList } from '../features/queues/queue-list';
import { TransferTable } from '../features/transfers/transfer-table';
import { TransferDetails } from '../features/transfers/transfer-details';
import { QueuePropertiesDialog, QueueFormValue } from '../features/queues/queue-properties-dialog';
import { TransferPropertiesDialog, TransferPropertiesFormValue } from '../features/transfers/transfer-properties-dialog';
import { NewTransferDialog, NewTransferFormValue } from '../features/transfers/new-transfer-dialog';
import { SettingsDialog } from '../features/settings/settings-dialog';
import { CaptchaDialog } from '../features/captcha/captcha-dialog';
import { SpeedGraphsTab } from '../features/speed-graphs/speed-graphs-tab';
import { LogTab } from '../features/log/log-tab';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';
import { QueueActionsService } from '../core/queue-actions.service';
import { TransferApi } from '../core/api/transfer-api';
import { AvailableTransferActions, TransfersStore, availableTransferActions } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { Queue, Transfer, TransferClass, NewTransferOptions } from '../core/api/models';

@Component({
  selector: 'app-shell',
  imports: [
    FormsModule,
    Toolbar,
    Select,
    Splitter,
    ButtonDirective,
    ButtonIcon,
    Tooltip,
    ConfirmDialog,
    Tabs,
    TabList,
    Tab,
    TabPanels,
    TabPanel,
    TransferDetails,
    SpeedGraphsTab,
    LogTab,
    QueueList,
    TransferTable,
    QueuePropertiesDialog,
    TransferPropertiesDialog,
    NewTransferDialog,
    SettingsDialog,
    CaptchaDialog,
  ],
  templateUrl: './shell.html',
  styleUrl: './shell.scss',
})
export class Shell {
  protected readonly polling = inject(PollingService);
  protected readonly allowedIntervals: number[] = [...this.polling.allowedIntervals];
  protected readonly actions = inject(TransferActionsService);
  private readonly transfersStore = inject(TransfersStore);
  private readonly queueActions = inject(QueueActionsService);
  protected readonly queuesStore = inject(QueuesStore);
  private readonly transferApi = inject(TransferApi);

  protected readonly transferClasses = signal<TransferClass[]>([]);
  protected readonly activeTab = signal('0');

  constructor() {
    this.transferApi.getTransferClasses().subscribe({
      next: (classes) => this.transferClasses.set(classes),
      error: (err) => console.error('Failed to load transfer classes', err),
    });
  }

  protected get available(): AvailableTransferActions {
    return availableTransferActions(this.transfersStore.transfers(), this.transfersStore.selectedUuids());
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

  protected readonly showTransferPropertiesDialog = signal(false);
  protected readonly transferPropertiesTarget = signal<Transfer | null>(null);

  protected openTransferPropertiesDialog(): void {
    const uuids = this.transfersStore.selectedUuids();
    const representative =
      uuids.length > 0 ? (this.transfersStore.transfers().find((t) => t.uuid === uuids[0]) ?? null) : null;
    this.transferPropertiesTarget.set(representative);
    this.showTransferPropertiesDialog.set(true);
  }

  protected onTransferPropertiesSave(value: TransferPropertiesFormValue): void {
    this.actions.updateProperties(value.comment, value.speedLimits);
  }

  protected readonly showNewTransferDialog = signal(false);

  protected openNewTransferDialog(): void {
    this.showNewTransferDialog.set(true);
  }

  protected onNewTransferSave(value: NewTransferFormValue): void {
    const options: NewTransferOptions = {
      upload: false,
      queueUuid: value.queueUuid,
      targetClass: value.targetClass,
      target: value.target,
      paused: value.paused,
      speedLimits: value.speedLimits,
    };
    if (value.file !== null) {
      this.actions.addTransferWithData(value.file, options);
    } else {
      this.actions.addTransfers(value.urls, options);
    }
  }

  protected readonly showSettingsDialog = signal(false);

  protected openSettingsDialog(): void {
    this.showSettingsDialog.set(true);
  }
}
