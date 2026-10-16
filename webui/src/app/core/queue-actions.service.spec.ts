import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { QueueActionsService } from './queue-actions.service';
import { TransferApi } from './api/transfer-api';
import { QueuesStore } from '../state/queues.store';

describe('QueueActionsService', () => {
  let createQueue: ReturnType<typeof vi.fn>;
  let setQueueProperties: ReturnType<typeof vi.fn>;
  let refreshNow: ReturnType<typeof vi.fn>;
  let service: QueueActionsService;

  beforeEach(() => {
    createQueue = vi.fn().mockReturnValue(of(undefined));
    setQueueProperties = vi.fn().mockReturnValue(of(undefined));
    refreshNow = vi.fn();

    TestBed.configureTestingModule({
      providers: [
        QueueActionsService,
        { provide: TransferApi, useValue: { createQueue, setQueueProperties } },
        { provide: QueuesStore, useValue: { refreshNow } },
      ],
    });
    service = TestBed.inject(QueueActionsService);
  });

  it('createQueue() calls TransferApi.createQueue() and refreshes on success', () => {
    service.createQueue({ name: 'Movies' });
    expect(createQueue).toHaveBeenCalledWith({ name: 'Movies' });
    expect(refreshNow).toHaveBeenCalled();
  });

  it('updateQueueProperties() calls TransferApi.setQueueProperties() and refreshes on success', () => {
    service.updateQueueProperties('q1', { name: 'Renamed' });
    expect(setQueueProperties).toHaveBeenCalledWith('q1', { name: 'Renamed' });
    expect(refreshNow).toHaveBeenCalled();
  });

  it('createQueue() does not refresh when the API call errors', () => {
    createQueue.mockReturnValue(throwError(() => new Error('boom')));
    service.createQueue({ name: 'Movies' });
    expect(refreshNow).not.toHaveBeenCalled();
  });
});
