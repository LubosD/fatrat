import { TestBed } from '@angular/core/testing';
import { ConfirmationService } from 'primeng/api';
import { Shell } from './shell';
import { QueuesStore } from '../state/queues.store';
import { TransfersStore } from '../state/transfers.store';
import { PollingService } from '../core/polling.service';
import { TransferActionsService } from '../core/transfer-actions.service';

function makeActionsStub() {
  return {
    resume: vi.fn(),
    forceResume: vi.fn(),
    pause: vi.fn(),
    deleteSelected: vi.fn(),
    deleteCompleted: vi.fn(),
    moveSelected: vi.fn(),
  };
}

function configure(
  selectedUuids: string[],
  actions: ReturnType<typeof makeActionsStub> = makeActionsStub(),
  intervalSeconds = 5,
) {
  TestBed.configureTestingModule({
    imports: [Shell],
    providers: [
      { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: vi.fn() } },
      {
        provide: TransfersStore,
        useValue: {
          transfers: () => [],
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
      ConfirmationService,
    ],
  });
  return actions;
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
    const actions = configure(['t1']);
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
});
