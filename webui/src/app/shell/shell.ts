import { StatusBar } from '../features/status-bar/status-bar';
import { Component, computed, inject, signal } from '@angular/core';
import { NgClass, NgTemplateOutlet } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { Toolbar } from 'primeng/toolbar';
import { Select } from 'primeng/select';
import { Splitter } from 'primeng/splitter';
import { Drawer } from 'primeng/drawer';
import { Menu } from 'primeng/menu';
import { MenuItem } from 'primeng/api';
import { ButtonDirective, ButtonIcon } from 'primeng/button';
import { Tooltip } from 'primeng/tooltip';
import { ConfirmDialog } from 'primeng/confirmdialog';
import { Tabs, TabList, Tab, TabPanels, TabPanel } from 'primeng/tabs';
import { QueueList } from '../features/queues/queue-list';
import { TransferTable } from '../features/transfers/transfer-table';
import { TransferCards } from '../features/transfers/transfer-cards';
import { TransferDetails } from '../features/transfers/transfer-details';
import { QueuePropertiesDialog, QueueFormValue } from '../features/queues/queue-properties-dialog';
import { TransferPropertiesDialog, TransferPropertiesFormValue } from '../features/transfers/transfer-properties-dialog';
import { NewTransferDialog, NewTransferFormValue } from '../features/transfers/new-transfer-dialog';
import { SettingsDialog } from '../features/settings/settings-dialog';
import { CaptchaDialog } from '../features/captcha/captcha-dialog';
import { SpeedGraphsTab } from '../features/speed-graphs/speed-graphs-tab';
import { LogTab } from '../features/log/log-tab';
import { ViewportService } from '../core/viewport.service';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';
import { QueueActionsService } from '../core/queue-actions.service';
import { TransferApi } from '../core/api/transfer-api';
import { AvailableTransferActions, TransfersStore, availableTransferActions } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { Queue, Transfer, TransferClass, NewTransferOptions } from '../core/api/models';

interface ToolbarAction {
  id: string;
  label: string;
  icon: string;
  disabled: () => boolean;
  run: () => void;
  // Shown directly in the mobile top bar; everything else goes into its overflow menu.
  primary?: boolean;
}

@Component({
  selector: 'app-shell',
  imports: [
    FormsModule,
    Toolbar,
    Select,
    Splitter,
    Drawer,
    Menu,
    NgTemplateOutlet,
    NgClass,
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
    TransferCards,
    QueuePropertiesDialog,
    TransferPropertiesDialog,
    NewTransferDialog,
    SettingsDialog,
    CaptchaDialog,
    StatusBar,
  ],
  templateUrl: './shell.html',
  styleUrl: './shell.scss',
})
export class Shell {
  protected readonly viewport = inject(ViewportService);
  protected readonly polling = inject(PollingService);
  protected readonly allowedIntervals: number[] = [...this.polling.allowedIntervals];
  protected readonly actions = inject(TransferActionsService);
  private readonly transfersStore = inject(TransfersStore);
  private readonly queueActions = inject(QueueActionsService);
  protected readonly queuesStore = inject(QueuesStore);
  private readonly transferApi = inject(TransferApi);

  protected readonly transferClasses = signal<TransferClass[]>([]);
  protected readonly activeTab = signal('0');
  protected readonly showQueueDrawer = signal(false);
  protected readonly selectedQueueName = computed(() => {
    const uuid = this.queuesStore.selectedQueueUuid();
    return this.queuesStore.queues().find((q) => q.uuid === uuid)?.name ?? '';
  });

  constructor() {
    this.loadTransferClasses();
  }

  private loadTransferClasses(): void {
    this.transferApi.getTransferClasses().subscribe({
      next: (classes) => this.transferClasses.set(classes),
      error: (err) => console.error('Failed to load transfer classes', err),
    });
  }

  protected readonly toolbarGroups: ToolbarAction[][] = [
    [
      { id: 'new-transfer', label: 'New Transfer', icon: 'pi-plus', primary: true, disabled: () => false, run: () => this.openNewTransferDialog() },
      { id: 'delete', label: 'Delete', icon: 'pi-trash', primary: true, disabled: () => !this.available.delete, run: () => this.actions.deleteSelected(false) },
      { id: 'delete-with-data', label: 'Delete with Data', icon: 'pi-eraser', disabled: () => !this.available.delete, run: () => this.actions.deleteSelected(true) },
      { id: 'delete-completed', label: 'Delete Completed', icon: 'pi-check-circle', disabled: () => false, run: () => this.actions.deleteCompleted() },
    ],
    [
      { id: 'resume', label: 'Resume', icon: 'pi-play', primary: true, disabled: () => !this.available.resume, run: () => this.actions.resume() },
      { id: 'force-resume', label: 'Force Resume', icon: 'pi-forward', disabled: () => !this.available.forceResume, run: () => this.actions.forceResume() },
      { id: 'pause', label: 'Pause', icon: 'pi-pause', primary: true, disabled: () => !this.available.pause, run: () => this.actions.pause() },
      { id: 'properties', label: 'Properties', icon: 'pi-pencil', disabled: () => !this.available.properties, run: () => this.openTransferPropertiesDialog() },
    ],
    [
      { id: 'move-top', label: 'Move to Top', icon: 'pi-angle-double-up', disabled: () => !this.available.moveUp, run: () => this.actions.moveSelected('top') },
      { id: 'move-up', label: 'Move Up', icon: 'pi-angle-up', disabled: () => !this.available.moveUp, run: () => this.actions.moveSelected('up') },
      { id: 'move-down', label: 'Move Down', icon: 'pi-angle-down', disabled: () => !this.available.moveDown, run: () => this.actions.moveSelected('down') },
      { id: 'move-bottom', label: 'Move to Bottom', icon: 'pi-angle-double-down', disabled: () => !this.available.moveDown, run: () => this.actions.moveSelected('bottom') },
    ],
    [
      { id: 'new-queue', label: 'New Queue', icon: 'pi-folder-plus', disabled: () => false, run: () => this.openNewQueueDialog() },
      { id: 'queue-properties', label: 'Queue Properties', icon: 'pi-sliders-h', disabled: () => !this.hasSelectedQueue, run: () => this.openQueuePropertiesDialog() },
    ],
    [{ id: 'settings', label: 'Settings', icon: 'pi-cog', disabled: () => false, run: () => this.openSettingsDialog() }],
  ];

  protected readonly primaryActions: ToolbarAction[] = this.toolbarGroups.flat().filter((a) => a.primary);

  protected readonly overflowItems = signal<MenuItem[]>([]);

  // Built on demand (not bound as a getter) so the items' disabled states reflect the selection
  // at the moment the menu opens, without handing the template a fresh array on every change detection.
  protected openOverflowMenu(event: Event, menu: Menu): void {
    // Two labelled groups: PrimeNG's Menu renders every top-level item as a (non-clickable) group
    // heading as soon as one top-level item has children, so the actions can't sit beside a submenu.
    const actions: MenuItem[] = [];
    for (const group of this.toolbarGroups) {
      const secondary = group.filter((a) => !a.primary);
      if (secondary.length === 0) {
        continue;
      }
      if (actions.length > 0) {
        actions.push({ separator: true });
      }
      for (const a of secondary) {
        actions.push({ label: a.label, icon: `pi ${a.icon}`, disabled: a.disabled(), command: () => a.run() });
      }
    }
    const items: MenuItem[] = [
      { label: 'Actions', items: actions },
      {
        label: 'Refresh every',
        items: this.allowedIntervals.map((seconds) => ({
          label: `${seconds} seconds`,
          icon: seconds === this.polling.intervalSeconds() ? 'pi pi-check' : undefined,
          command: () => this.polling.setIntervalSeconds(seconds),
        })),
      },
    ];
    this.overflowItems.set(items);
    menu.toggle(event);
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
    this.actions.updateProperties(value.comment, value.speedLimits, value.object);
  }

  protected readonly showNewTransferDialog = signal(false);

  protected openNewTransferDialog(): void {
    // Plugins can be installed or switched on while this page is open.
    this.loadTransferClasses();
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
