import { TestBed } from '@angular/core/testing';
import { By } from '@angular/platform-browser';
import { ConfirmationService, MessageService } from 'primeng/api';
import { Shell } from './shell';
import { QueuesStore } from '../state/queues.store';
import { TransfersStore } from '../state/transfers.store';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';
import { QueueActionsService } from '../core/queue-actions.service';
import { TransferApi } from '../core/api/transfer-api';
import { BrowseService } from '../core/browse.service';
import { LogService } from '../core/log.service';
import { SpeedGraphsTab } from '../features/speed-graphs/speed-graphs-tab';
import { LogTab } from '../features/log/log-tab';
import { SettingsDialog } from '../features/settings/settings-dialog';
import { PushSocketService } from '../core/push-socket.service';
import { TasksStore } from '../state/tasks.store';
import { TasksApi } from '../core/tasks-api.service';
import { CaptchaHttpService } from '../core/captcha-http.service';
import { ViewportService } from '../core/viewport.service';
import { signal } from '@angular/core';
import { Queue, Transfer } from '../core/api/models';
import { of, Subject } from 'rxjs';
import { PluginActions } from '../features/transfers/plugin-actions';

function makeQueue(overrides: Partial<Queue> = {}): Queue {
  return {
    uuid: 'q1',
    name: 'Default',
    defaultDirectory: '/downloads',
    moveDirectory: '',
    upAsDown: false,
    transferLimits: { down: 0, up: 0 },
    speedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'file.zip',
    state: 'Active',
    class: 'HttpDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/file.zip',
    dataPathIsDir: false,
    total: 1000,
    done: 250,
    comment: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

function makeActionsStub() {
  return {
    resume: vi.fn(),
    forceResume: vi.fn(),
    pause: vi.fn(),
    deleteSelected: vi.fn(),
    deleteCompleted: vi.fn(),
    moveSelected: vi.fn(),
    updateProperties: vi.fn(),
    addTransfers: vi.fn(),
    addTransferWithData: vi.fn(),
  };
}

function makeQueueActionsStub() {
  return { createQueue: vi.fn(), updateQueueProperties: vi.fn() };
}

function configure(
  selectedUuids: string[],
  actions: ReturnType<typeof makeActionsStub> = makeActionsStub(),
  intervalSeconds = 5,
  queues: Queue[] = [],
  selectedQueueUuid: string | null = null,
  queueActions: ReturnType<typeof makeQueueActionsStub> = makeQueueActionsStub(),
  transfers: Transfer[] = [],
) {
  const browseService = { list: vi.fn().mockReturnValue(of([])) };
  TestBed.configureTestingModule({
    imports: [Shell],
    providers: [
      { provide: QueuesStore, useValue: { queues: () => queues, selectedQueueUuid: () => selectedQueueUuid, selectQueue: vi.fn() } },
      {
        provide: TransfersStore,
        useValue: {
          transfers: () => transfers,
          selectedUuids: () => selectedUuids,
          isSelected: () => false,
          currentUuid: () => null,
          toggleSelection: vi.fn(),
          selectOnly: vi.fn(),
        },
      },
      {
        provide: PollingService,
        useValue: { intervalSeconds: () => intervalSeconds, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn(), ticks$: new Subject<number>() },
      },
      { provide: TransferActionsService, useValue: actions },
      { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      { provide: QueueActionsService, useValue: queueActions },
      {
        provide: TransferApi,
        useValue: {
          getTransferClasses: () => of([]),
          getTransferSpeedGraph: () => of([]),
          getQueueSpeedGraph: () => of([]),
          getStats: () => of({ total: { down: 0, up: 0 }, queues: {} }),
          getSettingsPages: () => of([]),
          getSettingsValues: () => of([]),
          setSettingsValues: () => of(undefined),
          applySettings: () => of(undefined),
        },
      },
      { provide: BrowseService, useValue: browseService },
      { provide: LogService, useValue: { getGlobalLog: () => of(''), getTransferLog: () => of('') } },
      { provide: PushSocketService, useValue: { captchaEvents$: new Subject(), taskEvents$: new Subject(), connected$: new Subject() } },
      { provide: TasksStore, useValue: { tasks: signal([]), openPrompts: signal([]), requested: signal(null), failures$: new Subject() } },
      { provide: TasksApi, useValue: {} },
      { provide: CaptchaHttpService, useValue: { submitSolution: () => of(undefined) } },
      ConfirmationService,
      MessageService,
    ],
  });
  return { actions, queueActions, browseService };
}

function findByTestId(root: HTMLElement, testId: string): HTMLButtonElement {
  return root.querySelector(`[data-testid="${testId}"]`) as HTMLButtonElement;
}

describe('Shell', () => {
  it('renders the queue list and transfer table', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('app-queue-list')).toBeTruthy();
    expect(fixture.nativeElement.querySelector('app-transfer-table')).toBeTruthy();
  });

  it('shows the current refresh interval in the toolbar select', async () => {
    configure([], makeActionsStub(), 15);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    const label = fixture.nativeElement.querySelector('.p-select-label');
    expect(label?.textContent).toContain('15');
  });

  it('disables selection-dependent action buttons when nothing is selected', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    for (const testId of ['action-resume', 'action-force-resume', 'action-pause', 'action-delete', 'action-delete-with-data', 'action-move-top', 'action-move-up', 'action-move-down', 'action-move-bottom']) {
      expect(findByTestId(fixture.nativeElement, testId).disabled).toBe(true);
    }
  });

  it('gates Resume/Pause on the selected transfers\' states and Move on their position, like the legacy UI', () => {
    const transfers = [makeTransfer({ uuid: 't1', state: 'Active' }), makeTransfer({ uuid: 't2', state: 'Paused' })];
    configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), transfers);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    const disabled = (testId: string) => findByTestId(fixture.nativeElement, testId).disabled;

    // t1 is Active and first in the queue.
    expect(disabled('action-resume')).toBe(true);
    expect(disabled('action-pause')).toBe(false);
    expect(disabled('action-force-resume')).toBe(false);
    expect(disabled('action-move-top')).toBe(true);
    expect(disabled('action-move-up')).toBe(true);
    expect(disabled('action-move-down')).toBe(false);
    expect(disabled('action-move-bottom')).toBe(false);
  });

  it('"Delete Completed" stays enabled regardless of selection', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(findByTestId(fixture.nativeElement, 'action-delete-completed').disabled).toBe(false);
  });

  it('enables selection-dependent action buttons once something is selected', () => {
    configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), [makeTransfer({ uuid: 't1', state: 'Paused' })]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(findByTestId(fixture.nativeElement, 'action-resume').disabled).toBe(false);
  });

  it('clicking each toolbar button calls the corresponding TransferActionsService method', () => {
    // A Failed transfer in the middle of the queue: every state- and position-gated action applies.
    const transfers = [makeTransfer({ uuid: 't0' }), makeTransfer({ uuid: 't1', state: 'Failed' }), makeTransfer({ uuid: 't2' })];
    const { actions } = configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), transfers);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    const root = fixture.nativeElement as HTMLElement;

    findByTestId(root, 'action-resume').click();
    expect(actions.resume).toHaveBeenCalled();
    findByTestId(root, 'action-force-resume').click();
    expect(actions.forceResume).toHaveBeenCalled();
    findByTestId(root, 'action-pause').click();
    expect(actions.pause).toHaveBeenCalled();
    findByTestId(root, 'action-delete').click();
    expect(actions.deleteSelected).toHaveBeenCalledWith(false);
    findByTestId(root, 'action-delete-with-data').click();
    expect(actions.deleteSelected).toHaveBeenCalledWith(true);
    findByTestId(root, 'action-delete-completed').click();
    expect(actions.deleteCompleted).toHaveBeenCalled();
    findByTestId(root, 'action-move-top').click();
    expect(actions.moveSelected).toHaveBeenCalledWith('top');
    findByTestId(root, 'action-move-up').click();
    expect(actions.moveSelected).toHaveBeenCalledWith('up');
    findByTestId(root, 'action-move-down').click();
    expect(actions.moveSelected).toHaveBeenCalledWith('down');
    findByTestId(root, 'action-move-bottom').click();
    expect(actions.moveSelected).toHaveBeenCalledWith('bottom');
  });

  it('renders a confirm dialog host', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('p-confirmdialog')).toBeTruthy();
  });

  it('"New Queue" opens the queue dialog with queue set to null', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    findByTestId(fixture.nativeElement, 'action-new-queue').click();
    fixture.detectChanges();

    expect(fixture.componentInstance['queueDialogTarget']()).toBeNull();
    expect(fixture.componentInstance['showQueueDialog']()).toBe(true);
  });

  it('"Queue Properties" is disabled when no queue is selected', () => {
    const queue = makeQueue({ uuid: 'q1', name: 'Movies' });
    configure([], makeActionsStub(), 5, [queue], null);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    expect(findByTestId(fixture.nativeElement, 'action-queue-properties').disabled).toBe(true);
  });

  it('"Queue Properties" is enabled and opens the dialog prefilled when a queue is selected', () => {
    const queue = makeQueue({ uuid: 'q1', name: 'Movies' });
    configure([], makeActionsStub(), 5, [queue], 'q1');
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    expect(findByTestId(fixture.nativeElement, 'action-queue-properties').disabled).toBe(false);
    findByTestId(fixture.nativeElement, 'action-queue-properties').click();
    fixture.detectChanges();
    expect(fixture.componentInstance['queueDialogTarget']()).toEqual(queue);
  });

  it('saving the queue dialog with queueDialogTarget null calls createQueue()', () => {
    const { queueActions } = configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    fixture.componentInstance['onQueueDialogSave']({
      name: 'New',
      defaultDirectory: '',
      moveDirectory: '',
      speedLimits: { down: 0, up: 0 },
      transferLimits: { down: 0, up: 0 },
      upAsDown: false,
    });
    expect(queueActions.createQueue).toHaveBeenCalled();
  });

  it('saving the queue dialog with a queueDialogTarget set calls updateQueueProperties()', () => {
    const queue = makeQueue({ uuid: 'q1' });
    const { queueActions } = configure([], makeActionsStub(), 5, [queue], 'q1');
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    fixture.componentInstance['openQueuePropertiesDialog']();

    fixture.componentInstance['onQueueDialogSave']({
      name: 'Renamed',
      defaultDirectory: '',
      moveDirectory: '',
      speedLimits: { down: 0, up: 0 },
      transferLimits: { down: 0, up: 0 },
      upAsDown: false,
    });
    expect(queueActions.updateQueueProperties).toHaveBeenCalledWith('q1', expect.objectContaining({ name: 'Renamed' }));
  });

  it('"Properties" is disabled with nothing selected', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    expect(findByTestId(fixture.nativeElement, 'action-properties').disabled).toBe(true);
  });

  it('"Properties" is enabled and opens the dialog prefilled from the first selected transfer when something is selected', () => {
    const t1 = makeTransfer({ uuid: 't1', comment: 'hello' });
    configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), [t1]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    expect(findByTestId(fixture.nativeElement, 'action-properties').disabled).toBe(false);
    findByTestId(fixture.nativeElement, 'action-properties').click();
    fixture.detectChanges();
    expect(fixture.componentInstance['transferPropertiesTarget']()).toEqual(t1);
  });

  it('saving the transfer properties dialog calls TransferActionsService.updateProperties()', () => {
    const { actions } = configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), [makeTransfer({ uuid: 't1' })]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    fixture.componentInstance['onTransferPropertiesSave']({ comment: 'new', speedLimits: { down: 1, up: 2 } });
    expect(actions.updateProperties).toHaveBeenCalledWith('new', { down: 1, up: 2 }, undefined);
  });

  it('"New Transfer" opens the new-transfer dialog', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    findByTestId(fixture.nativeElement, 'action-new-transfer').click();
    fixture.detectChanges();

    expect(fixture.componentInstance['showNewTransferDialog']()).toBe(true);
  });

  it('fetches transfer classes on construction and passes them to the new-transfer dialog', () => {
    const getTransferClasses = vi.fn().mockReturnValue(of([{ mode: 'Download', shortName: 'http', longName: 'HTTP Download' }]));
    TestBed.configureTestingModule({
      imports: [Shell],
      providers: [
        { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: vi.fn() } },
        {
          provide: TransfersStore,
          useValue: { transfers: () => [], selectedUuids: () => [], isSelected: () => false, currentUuid: () => null, toggleSelection: vi.fn(), selectOnly: vi.fn() },
        },
        { provide: PollingService, useValue: { intervalSeconds: () => 5, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn(), ticks$: new Subject<number>() } },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
        { provide: QueueActionsService, useValue: makeQueueActionsStub() },
        {
          provide: TransferApi,
          useValue: {
            getTransferClasses,
            getTransferSpeedGraph: () => of([]),
            getQueueSpeedGraph: () => of([]),
            getStats: () => of({ total: { down: 0, up: 0 }, queues: {} }),
            getSettingsPages: () => of([]),
            getSettingsValues: () => of([]),
            setSettingsValues: () => of(undefined),
            applySettings: () => of(undefined),
          },
        },
        { provide: BrowseService, useValue: { list: () => of([]) } },
        { provide: LogService, useValue: { getGlobalLog: () => of(''), getTransferLog: () => of('') } },
        { provide: PushSocketService, useValue: { captchaEvents$: new Subject(), taskEvents$: new Subject(), connected$: new Subject() } },
        { provide: TasksStore, useValue: { tasks: signal([]), openPrompts: signal([]), requested: signal(null), failures$: new Subject() } },
        { provide: TasksApi, useValue: {} },
        { provide: CaptchaHttpService, useValue: { submitSolution: () => of(undefined) } },
        ConfirmationService,
        MessageService,
      ],
    });
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(getTransferClasses).toHaveBeenCalled();
    expect(fixture.componentInstance['transferClasses']()).toEqual([{ mode: 'Download', shortName: 'http', longName: 'HTTP Download' }]);
  });

  it('re-fetches the transfer classes each time the new-transfer dialog opens, so a freshly loaded plugin shows up', () => {
    const http = { mode: 'Download', shortName: 'http', longName: 'HTTP Download' };
    const plugin = { mode: 'Download', shortName: 'warforum', longName: 'Warforum free' };
    const getTransferClasses = vi.fn().mockReturnValueOnce(of([http])).mockReturnValue(of([http, plugin]));
    TestBed.configureTestingModule({
      imports: [Shell],
      providers: [
        { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: vi.fn() } },
        {
          provide: TransfersStore,
          useValue: { transfers: () => [], selectedUuids: () => [], isSelected: () => false, currentUuid: () => null, toggleSelection: vi.fn(), selectOnly: vi.fn() },
        },
        { provide: PollingService, useValue: { intervalSeconds: () => 5, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn(), ticks$: new Subject<number>() } },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
        { provide: QueueActionsService, useValue: makeQueueActionsStub() },
        {
          provide: TransferApi,
          useValue: {
            getTransferClasses,
            getTransferSpeedGraph: () => of([]),
            getQueueSpeedGraph: () => of([]),
            getStats: () => of({ total: { down: 0, up: 0 }, queues: {} }),
            getSettingsPages: () => of([]),
            getSettingsValues: () => of([]),
            setSettingsValues: () => of(undefined),
            applySettings: () => of(undefined),
          },
        },
        { provide: BrowseService, useValue: { list: () => of([]) } },
        { provide: LogService, useValue: { getGlobalLog: () => of(''), getTransferLog: () => of('') } },
        { provide: PushSocketService, useValue: { captchaEvents$: new Subject(), taskEvents$: new Subject(), connected$: new Subject() } },
        { provide: TasksStore, useValue: { tasks: signal([]), openPrompts: signal([]), requested: signal(null), failures$: new Subject() } },
        { provide: TasksApi, useValue: {} },
        { provide: CaptchaHttpService, useValue: { submitSolution: () => of(undefined) } },
        ConfirmationService,
        MessageService,
      ],
    });
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    expect(fixture.componentInstance['transferClasses']()).toEqual([http]);

    fixture.componentInstance['openNewTransferDialog']();

    expect(getTransferClasses).toHaveBeenCalledTimes(2);
    expect(fixture.componentInstance['transferClasses']()).toEqual([http, plugin]);
  });

  it('saving the new-transfer dialog with a file calls addTransferWithData()', () => {
    const { actions } = configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    const file = new File(['data'], 'test.torrent');
    fixture.componentInstance['onNewTransferSave']({
      urls: [],
      file,
      queueUuid: 'q1',
      targetClass: '',
      target: '/downloads',
      paused: false,
      speedLimits: { down: 0, up: 0 },
    });

    expect(actions.addTransferWithData).toHaveBeenCalledWith(
      file,
      expect.objectContaining({ upload: false, queueUuid: 'q1', target: '/downloads' }),
    );
    expect(actions.addTransfers).not.toHaveBeenCalled();
  });

  it('saving the new-transfer dialog with urls and no file calls addTransfers()', () => {
    const { actions } = configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    fixture.componentInstance['onNewTransferSave']({
      urls: ['http://example.com/a'],
      file: null,
      queueUuid: 'q1',
      targetClass: '',
      target: '/downloads',
      paused: false,
      speedLimits: { down: 0, up: 0 },
    });

    expect(actions.addTransfers).toHaveBeenCalledWith(
      ['http://example.com/a'],
      expect.objectContaining({ upload: false, queueUuid: 'q1', target: '/downloads' }),
    );
    expect(actions.addTransferWithData).not.toHaveBeenCalled();
  });

  it('renders Transfers and Details tabs', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    const root = fixture.nativeElement as HTMLElement;
    expect(Array.from(root.querySelectorAll('p-tab')).some((el) => el.textContent?.includes('Transfers'))).toBe(true);
    expect(Array.from(root.querySelectorAll('p-tab')).some((el) => el.textContent?.includes('Details'))).toBe(true);
    expect(root.querySelector('app-transfer-table')).toBeTruthy();
  });

  it('does not call BrowseService.list when a directory-backed transfer is selected but the Details tab is never opened', async () => {
    const dirTransfer = makeTransfer({ uuid: 't1', dataPathIsDir: true });
    const { browseService } = configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), [dirTransfer]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(browseService.list).not.toHaveBeenCalled();
  });

  it('activating the Details tab renders TransferDetails and triggers BrowseService.list', async () => {
    const dirTransfer = makeTransfer({ uuid: 't1', dataPathIsDir: true });
    const { browseService } = configure(['t1'], makeActionsStub(), 5, [], null, makeQueueActionsStub(), [dirTransfer]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    const root = fixture.nativeElement as HTMLElement;
    const detailsTab = Array.from(root.querySelectorAll('p-tab')).find((el) => el.textContent?.includes('Details')) as HTMLElement;
    expect(detailsTab).toBeTruthy();
    detailsTab.click();
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(root.querySelector('app-transfer-details')).toBeTruthy();
    expect(browseService.list).toHaveBeenCalled();
  });

  it('renders Speed Graphs and Log tabs, and only the active tab is marked active', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    const root = fixture.nativeElement as HTMLElement;
    const tabs = Array.from(root.querySelectorAll('p-tab'));
    expect(tabs.some((el) => el.textContent?.includes('Speed Graphs'))).toBe(true);
    expect(tabs.some((el) => el.textContent?.includes('Log'))).toBe(true);

    // Transfers tab (value "0") is selected by default. Both new panels are [lazy]="true", and
    // PrimeNG's TabPanel never instantiates lazy content until that panel is activated at least
    // once (confirmed against node_modules/primeng/fesm2022/primeng-tabs.mjs's shouldRender()),
    // matching the established Phase 3a "Details tab is never opened" behavior tested above — so
    // neither app-speed-graphs-tab nor app-log-tab exists in the DOM yet.
    expect(root.querySelector('app-speed-graphs-tab')).toBeNull();
    expect(root.querySelector('app-log-tab')).toBeNull();

    // Activate Speed Graphs (value "2"): its content renders for the first time with
    // active=true, while Log (value "3") remains unrendered.
    fixture.componentInstance['activeTab'].set('2');
    fixture.detectChanges();

    const speedGraphsTabDebugEl = fixture.debugElement.query(By.directive(SpeedGraphsTab));
    expect(speedGraphsTabDebugEl).toBeTruthy();
    expect(speedGraphsTabDebugEl.componentInstance.active()).toBe(true);
    expect(root.querySelector('app-log-tab')).toBeNull();

    // Activate Log (value "3"): Speed Graphs' component instance persists (lazy content, once
    // rendered, is only hidden — not destroyed) but its `active` input flips to false, proving
    // the [active] binding tracks the current tab rather than being a one-shot initial value.
    fixture.componentInstance['activeTab'].set('3');
    fixture.detectChanges();

    const logTabDebugEl = fixture.debugElement.query(By.directive(LogTab));
    expect(logTabDebugEl).toBeTruthy();
    expect(logTabDebugEl.componentInstance.active()).toBe(true);
    expect(speedGraphsTabDebugEl.componentInstance.active()).toBe(false);
  });

  it('"Settings" opens the settings dialog', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    findByTestId(fixture.nativeElement, 'action-settings').click();
    fixture.detectChanges();

    expect(fixture.componentInstance['showSettingsDialog']()).toBe(true);
  });

  it('mounts the captcha dialog', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('app-captcha-dialog')).toBeTruthy();
  });
});

describe('Shell (mobile)', () => {
  function configureMobile() {
    configure([], makeActionsStub(), 5, [makeQueue({ uuid: 'q1', name: 'Movies' })], 'q1');
    TestBed.overrideProvider(ViewportService, { useValue: { isMobile: signal(true) } });
  }

  it('renders transfer cards instead of the table or splitter, and shows the selected queue name', () => {
    configureMobile();
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    const el: HTMLElement = fixture.nativeElement;

    expect(el.querySelector('p-splitter')).toBeNull();
    expect(el.querySelector('app-transfer-table')).toBeNull();
    expect(el.querySelector('app-transfer-cards')).toBeTruthy();
    expect(findByTestId(el, 'mobile-queue-name').textContent).toContain('Movies');
  });

  it('opens the queue drawer from the hamburger button and closes it when a queue is chosen', () => {
    configureMobile();
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    expect(fixture.componentInstance['showQueueDrawer']()).toBe(false);

    findByTestId(fixture.nativeElement, 'mobile-queues-button').click();
    fixture.detectChanges();
    expect(fixture.componentInstance['showQueueDrawer']()).toBe(true);

    const item = document.querySelector('[data-testid="queue-item"]') as HTMLElement;
    expect(item).toBeTruthy();
    item.click();
    fixture.detectChanges();
    expect(fixture.componentInstance['showQueueDrawer']()).toBe(false);
  });

  it('renders the queue list exactly once', () => {
    configureMobile();
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    findByTestId(fixture.nativeElement, 'mobile-queues-button').click();
    fixture.detectChanges();

    expect(document.querySelectorAll('app-queue-list')).toHaveLength(1);
  });

  it('shows only the primary actions in the top bar and hides the desktop toolbar', () => {
    configureMobile();
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    const el: HTMLElement = fixture.nativeElement;

    expect(el.querySelector('p-toolbar')).toBeNull();
    for (const id of ['action-new-transfer', 'action-resume', 'action-pause', 'action-delete']) {
      expect(findByTestId(el, id)).toBeTruthy();
    }
    for (const id of ['action-move-top', 'action-settings', 'action-force-resume']) {
      expect(findByTestId(el, id)).toBeNull();
    }
  });

  it('puts every other action, with its disabled state, plus the refresh interval into the overflow menu', () => {
    configureMobile();
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    findByTestId(fixture.nativeElement, 'mobile-more-button').click();
    fixture.detectChanges();

    const groups = fixture.componentInstance['overflowItems']();
    const items = groups[0].items!;
    const labels = items.filter((i) => !i.separator).map((i) => i.label);
    expect(labels).toEqual([
      'Delete with Data', 'Delete Completed', 'Force Resume', 'Properties',
      'Move to Top', 'Move Up', 'Move Down', 'Move to Bottom',
      'New Queue', 'Queue Properties', 'Settings',
    ]);
    expect(groups.map((g) => g.label)).toEqual(['Actions', 'Refresh every']);
    const byLabel = (label: string) => items.find((i) => i.label === label)!;
    expect(byLabel('Delete with Data').disabled).toBe(true);
    expect(byLabel('Delete Completed').disabled).toBe(false);
    expect(byLabel('Queue Properties').disabled).toBe(false);
  });

  it('runs an overflow action and changes the refresh interval from the menu', () => {
    configureMobile();
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    findByTestId(fixture.nativeElement, 'mobile-more-button').click();
    const [actionGroup, intervalGroup] = fixture.componentInstance['overflowItems']();

    actionGroup.items!.find((i) => i.label === 'Settings')!.command!({} as never);
    expect(fixture.componentInstance['showSettingsDialog']()).toBe(true);

    const interval = intervalGroup.items!;
    expect(interval.map((i) => i.label)).toEqual(['2 seconds', '5 seconds', '15 seconds', '30 seconds']);
    expect(interval.find((i) => i.label === '5 seconds')!.icon).toBe('pi pi-check');
    interval[2].command!({} as never);
    expect(TestBed.inject(PollingService).setIntervalSeconds).toHaveBeenCalledWith(15);
  });
});
