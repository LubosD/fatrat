import { StatusBar } from '../features/status-bar/status-bar';
import { Component, computed, DOCUMENT, HostListener, inject, signal, ViewChild } from '@angular/core';
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
import { Toast } from 'primeng/toast';
import { Tabs, TabList, Tab, TabPanels, TabPanel } from 'primeng/tabs';
import { QueueList } from '../features/queues/queue-list';
import { TransferTable } from '../features/transfers/transfer-table';
import { TransferCards } from '../features/transfers/transfer-cards';
import { TransferDetails } from '../features/transfers/transfer-details';
import { QueuePropertiesDialog, QueueFormValue } from '../features/queues/queue-properties-dialog';
import { TransferPropertiesDialog } from '../features/transfers/transfer-properties-dialog';
import { NewTransferDialog } from '../features/transfers/new-transfer-dialog';
import { SettingsDialog } from '../features/settings/settings-dialog';
import { CaptchaDialog } from '../features/captcha/captcha-dialog';
import { TaskPromptDialog } from '../features/tasks/task-prompt-dialog';
import { SpeedGraphsTab } from '../features/speed-graphs/speed-graphs-tab';
import { LogTab } from '../features/log/log-tab';
import { ViewportService } from '../core/viewport.service';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';
import { QueueActionsService } from '../core/queue-actions.service';
import { TransferApi } from '../core/api/transfer-api';
import { AvailableTransferActions, TransfersStore, availableTransferActions } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { Hotkey, ariaKeyShortcuts, hasCommandModifier, isOverlayOpen, isTypingTarget, matchesHotkey } from './hotkeys';
import { Queue, Transfer, TransferClass, NewTransferOptions } from '../core/api/models';

interface ToolbarAction {
  id: string;
  label: string;
  icon: string;
  disabled: () => boolean;
  run: () => void;
  // Shown directly in the mobile top bar; everything else goes into its overflow menu.
  primary?: boolean;
  // The first one is shown in the badge and tooltip; the rest are aliases.
  hotkeys?: Hotkey[];
}

const TAB_KEYS = ['1', '2', '3', '4'];

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
    Toast,
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
    TaskPromptDialog,
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
      { id: 'new-transfer', label: 'New Transfer', icon: 'pi-plus', primary: true, hotkeys: [{ key: 'n', label: 'N' }], disabled: () => false, run: () => this.openNewTransferDialog() },
      { id: 'delete', label: 'Delete', icon: 'pi-trash', primary: true, hotkeys: [{ key: 'd', label: 'D' }, { key: 'Delete', shift: false, label: 'Del' }], disabled: () => !this.available.delete, run: () => this.actions.deleteSelected(false) },
      { id: 'delete-with-data', label: 'Delete with Data', icon: 'pi-eraser', hotkeys: [{ key: 'D', label: 'Shift+D' }, { key: 'Delete', shift: true, label: 'Shift+Del' }], disabled: () => !this.available.delete, run: () => this.actions.deleteSelected(true) },
      { id: 'delete-completed', label: 'Delete Completed', icon: 'pi-check-circle', hotkeys: [{ key: 'c', label: 'C' }], disabled: () => false, run: () => this.actions.deleteCompleted() },
    ],
    [
      { id: 'resume', label: 'Resume', icon: 'pi-play', primary: true, hotkeys: [{ key: 'r', label: 'R' }], disabled: () => !this.available.resume, run: () => this.actions.resume() },
      { id: 'force-resume', label: 'Force Resume', icon: 'pi-forward', hotkeys: [{ key: 'f', label: 'F' }], disabled: () => !this.available.forceResume, run: () => this.actions.forceResume() },
      { id: 'pause', label: 'Pause', icon: 'pi-pause', primary: true, hotkeys: [{ key: 'p', label: 'P' }], disabled: () => !this.available.pause, run: () => this.actions.pause() },
      { id: 'properties', label: 'Properties', icon: 'pi-pencil', hotkeys: [{ key: 'e', label: 'E' }], disabled: () => !this.available.properties, run: () => this.openTransferPropertiesDialog() },
    ],
    [
      { id: 'move-top', label: 'Move to Top', icon: 'pi-angle-double-up', hotkeys: [{ key: '{', label: '{' }], disabled: () => !this.available.moveUp, run: () => this.actions.moveSelected('top') },
      { id: 'move-up', label: 'Move Up', icon: 'pi-angle-up', hotkeys: [{ key: '[', label: '[', repeat: true }], disabled: () => !this.available.moveUp, run: () => this.actions.moveSelected('up') },
      { id: 'move-down', label: 'Move Down', icon: 'pi-angle-down', hotkeys: [{ key: ']', label: ']', repeat: true }], disabled: () => !this.available.moveDown, run: () => this.actions.moveSelected('down') },
      { id: 'move-bottom', label: 'Move to Bottom', icon: 'pi-angle-double-down', hotkeys: [{ key: '}', label: '}' }], disabled: () => !this.available.moveDown, run: () => this.actions.moveSelected('bottom') },
    ],
    [
      { id: 'new-queue', label: 'New Queue', icon: 'pi-folder-plus', hotkeys: [{ key: 'N', label: 'Shift+N' }], disabled: () => false, run: () => this.openNewQueueDialog() },
      { id: 'queue-properties', label: 'Queue Properties', icon: 'pi-sliders-h', hotkeys: [{ key: 'E', label: 'Shift+E' }], disabled: () => !this.hasSelectedQueue, run: () => this.openQueuePropertiesDialog() },
    ],
    [{ id: 'settings', label: 'Settings', icon: 'pi-cog', hotkeys: [{ key: 'o', label: 'O' }], disabled: () => false, run: () => this.openSettingsDialog() }],
  ];

  protected readonly primaryActions: ToolbarAction[] = this.toolbarGroups.flat().filter((a) => a.primary);

  protected readonly showHotkeys = signal(false);
  // The badges are positioned over the desktop toolbar and tab headers, which the mobile layout lacks.
  protected readonly hotkeyBadgesVisible = computed(() => this.showHotkeys() && !this.viewport.isMobile());
  protected readonly ariaKeyShortcuts = ariaKeyShortcuts;

  // Badges sit under 40px-wide buttons, so "Shift+N" is shortened to "⇧N".
  protected badgeLabel(hotkey: Hotkey): string {
    return hotkey.label.replace('Shift+', '⇧');
  }

  protected tooltipFor(action: ToolbarAction): string {
    const key = action.hotkeys?.[0]?.label;
    return key ? `${action.label} (${key})` : action.label;
  }

  protected toggleHotkeys(): void {
    this.showHotkeys.update((v) => !v);
  }

  @HostListener('document:click', ['$event'])
  protected onDocumentClick(event: MouseEvent): void {
    // The Shortcuts button toggles on its own; hiding here first would turn it straight back on.
    if (event.target instanceof Element && event.target.closest('[data-testid="action-hotkeys"]')) return;
    this.showHotkeys.set(false);
  }

  private readonly document = inject(DOCUMENT);
  private readonly allActions = this.toolbarGroups.flat();

  @ViewChild(ConfirmDialog) private confirmDialog?: ConfirmDialog;

  /** The confirmation during which the user pressed Tab; a new confirmation is a different object. */
  private confirmTabbedIn: unknown = null;

  /**
   * Enter accepts the shared confirmation, wherever focus landed, so a hotkey delete is `D` then
   * Enter. PrimeNG lands the initial focus on Reject (its `defaultFocus` selector matches nothing),
   * so a focused non-accept button only keeps its own Enter once the user moved focus there
   * themselves with Tab.
   */
  private acceptConfirmOnEnter(event: KeyboardEvent): boolean {
    const dialog = this.confirmDialog;
    if (!dialog?.visible) {
      this.confirmTabbedIn = null;
      return false;
    }
    if (event.key === 'Tab') {
      this.confirmTabbedIn = dialog.confirmation ?? null;
      return false;
    }
    if (event.key !== 'Enter' || hasCommandModifier(event)) return false;
    const button = event.target instanceof Element ? event.target.closest('button') : null;
    const movedByUser = this.confirmTabbedIn !== null && this.confirmTabbedIn === dialog.confirmation;
    if (movedByUser && button && !button.classList.contains('p-confirmdialog-accept-button')) return false;
    // Also stops the focused accept button's own Enter-click, which would accept a second time.
    event.preventDefault();
    if (!event.repeat) dialog.onAccept();
    return true;
  }

  /**
   * PrimeNG's initial focus lands on Reject (the first footer button; its `defaultFocus` selector
   * matches nothing), while Enter accepts. Move it to Accept unless the user has tabbed around.
   */
  @HostListener('document:focusin', ['$event'])
  protected onDocumentFocusin(event: FocusEvent): void {
    const dialog = this.confirmDialog;
    if (!dialog?.visible || !(event.target instanceof Element)) return;
    if (!event.target.classList.contains('p-confirmdialog-reject-button')) return;
    if (this.confirmTabbedIn !== null && this.confirmTabbedIn === dialog.confirmation) return;
    event.target.closest('.p-confirmdialog')?.querySelector<HTMLElement>('.p-confirmdialog-accept-button')?.focus();
  }

  @HostListener('document:keydown', ['$event'])
  protected onDocumentKeydown(event: KeyboardEvent): void {
    if (this.acceptConfirmOnEnter(event)) return;
    if (event.defaultPrevented || hasCommandModifier(event) || isTypingTarget(event.target) || isOverlayOpen(this.document)) {
      return;
    }
    if (event.key === '?') {
      event.preventDefault();
      if (!event.repeat) this.toggleHotkeys();
      return;
    }
    if (event.key === 'Escape') {
      this.showHotkeys.set(false);
      return;
    }
    let run: () => void;
    let repeats = false;
    const tab = TAB_KEYS.indexOf(event.key);
    if (tab >= 0) {
      run = () => this.activeTab.set(String(tab));
    } else {
      let action: ToolbarAction | undefined;
      let hotkey: Hotkey | undefined;
      for (const a of this.allActions) {
        hotkey = a.hotkeys?.find((h) => matchesHotkey(event, h));
        if (hotkey) {
          action = a;
          break;
        }
      }
      if (!action || !hotkey) return;
      repeats = hotkey.repeat === true;
      const matched = action;
      // A disabled action still swallows its key, like clicking a greyed-out button.
      run = () => {
        if (!matched.disabled()) matched.run();
      };
    }
    event.preventDefault();
    if (event.repeat && !repeats) return;
    this.showHotkeys.set(false);
    run();
  }

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
  protected readonly transferPropertiesCount = signal(0);

  protected openTransferPropertiesDialog(): void {
    const uuids = this.transfersStore.selectedUuids();
    const representative =
      uuids.length > 0 ? (this.transfersStore.transfers().find((t) => t.uuid === uuids[0]) ?? null) : null;
    this.transferPropertiesTarget.set(representative);
    this.transferPropertiesCount.set(uuids.length);
    this.showTransferPropertiesDialog.set(true);
  }

  protected readonly showNewTransferDialog = signal(false);

  protected openNewTransferDialog(): void {
    // Plugins can be installed or switched on while this page is open.
    this.loadTransferClasses();
    this.showNewTransferDialog.set(true);
  }

  protected readonly showSettingsDialog = signal(false);

  protected openSettingsDialog(): void {
    this.showSettingsDialog.set(true);
  }
}
