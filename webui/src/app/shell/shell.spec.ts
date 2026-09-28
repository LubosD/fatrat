import { TestBed } from '@angular/core/testing';
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

describe('Shell', () => {
  it('renders the queue list and transfer table', () => {
    TestBed.configureTestingModule({
      imports: [Shell],
      providers: [
        { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: vi.fn() } },
        { provide: TransfersStore, useValue: { transfers: () => [] } },
        { provide: PollingService, useValue: { intervalSeconds: () => 5, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn() } },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('app-queue-list')).toBeTruthy();
    expect(fixture.nativeElement.querySelector('app-transfer-table')).toBeTruthy();
  });

  it('shows the current refresh interval in the toolbar select', async () => {
    TestBed.configureTestingModule({
      imports: [Shell],
      providers: [
        { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: vi.fn() } },
        { provide: TransfersStore, useValue: { transfers: () => [] } },
        { provide: PollingService, useValue: { intervalSeconds: () => 15, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: vi.fn() } },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(Shell);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    // Read through the rendered DOM rather than reaching into the component
    // instance's protected `polling` field — PrimeNG's Select renders its
    // current label text into a `.p-select-label` element.
    const label = fixture.nativeElement.querySelector('.p-select-label');
    expect(label?.textContent).toContain('15');
  });
});
