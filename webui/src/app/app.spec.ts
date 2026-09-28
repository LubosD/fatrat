import { TestBed } from '@angular/core/testing';
import { ConfirmationService } from 'primeng/api';
import { App } from './app';
import { QueuesStore } from './state/queues.store';
import { TransfersStore } from './state/transfers.store';
import { PollingService } from './core/polling.service';
import { TransferActionsService } from './core/transfer-actions.service';
import { QueueActionsService } from './core/queue-actions.service';

describe('App', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [App],
      providers: [
        { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: () => {} } },
        {
          provide: TransfersStore,
          useValue: {
            transfers: () => [],
            selectedUuids: () => [],
            isSelected: () => false,
            toggleSelection: () => {},
            selectOnly: () => {},
          },
        },
        { provide: PollingService, useValue: { intervalSeconds: () => 5, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: () => {} } },
        {
          provide: TransferActionsService,
          useValue: {
            resume: vi.fn(),
            forceResume: vi.fn(),
            pause: vi.fn(),
            deleteSelected: vi.fn(),
            deleteCompleted: vi.fn(),
            moveSelected: vi.fn(),
          },
        },
        { provide: QueueActionsService, useValue: { createQueue: vi.fn(), updateQueueProperties: vi.fn() } },
        ConfirmationService,
      ],
    });
  });

  it('creates the app and renders the shell', () => {
    const fixture = TestBed.createComponent(App);
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('app-shell')).toBeTruthy();
  });
});
