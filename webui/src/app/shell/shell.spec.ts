import { TestBed } from '@angular/core/testing';
import { ConfirmationService } from 'primeng/api';
import { Shell } from './shell';
import { QueuesStore } from '../state/queues.store';
import { TransfersStore } from '../state/transfers.store';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';
import { QueueActionsService } from '../core/queue-actions.service';
import { TransferApi } from '../core/api/transfer-api';
import { BrowseService } from '../core/browse.service';
import { Queue, Transfer } from '../core/api/models';
import { of } from 'rxjs';

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
          toggleSelection: vi.fn(),
          selectOnly: vi.fn(),
        },
      },
      {
        provide: PollingService,
        useValue: { intervalSeconds: () => intervalSeconds, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn() },
      },
      { provide: TransferActionsService, useValue: actions },
      { provide: QueueActionsService, useValue: queueActions },
      { provide: TransferApi, useValue: { getTransferClasses: () => of([]) } },
      { provide: BrowseService, useValue: browseService },
      ConfirmationService,
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

  it('"Delete Completed" stays enabled regardless of selection', () => {
    configure([]);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(findByTestId(fixture.nativeElement, 'action-delete-completed').disabled).toBe(false);
  });

  it('enables selection-dependent action buttons once something is selected', () => {
    configure(['t1']);
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(findByTestId(fixture.nativeElement, 'action-resume').disabled).toBe(false);
  });

  it('clicking each toolbar button calls the corresponding TransferActionsService method', () => {
    const { actions } = configure(['t1']);
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
    expect(actions.updateProperties).toHaveBeenCalledWith('new', { down: 1, up: 2 });
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
          useValue: { transfers: () => [], selectedUuids: () => [], isSelected: () => false, toggleSelection: vi.fn(), selectOnly: vi.fn() },
        },
        { provide: PollingService, useValue: { intervalSeconds: () => 5, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn() } },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: QueueActionsService, useValue: makeQueueActionsStub() },
        { provide: TransferApi, useValue: { getTransferClasses } },
        { provide: BrowseService, useValue: { list: () => of([]) } },
        ConfirmationService,
      ],
    });
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(getTransferClasses).toHaveBeenCalled();
    expect(fixture.componentInstance['transferClasses']()).toEqual([{ mode: 'Download', shortName: 'http', longName: 'HTTP Download' }]);
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
});
