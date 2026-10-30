import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { QueueActionsService } from './queue-actions.service';
import { TransferApi } from './api/transfer-api';
import { ConfirmationService, MessageService } from '@openng/optimus-ui/api';
import { ApiError } from './api/rest-client';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';

describe('QueueActionsService', () => {
  let createQueue: ReturnType<typeof vi.fn>;
  let setQueueProperties: ReturnType<typeof vi.fn>;
  let refreshNow: ReturnType<typeof vi.fn>;
  let deleteQueue: ReturnType<typeof vi.fn>;
  let confirm: ReturnType<typeof vi.fn>;
  let transfersRefresh: ReturnType<typeof vi.fn>;
  let messagesAdd: ReturnType<typeof vi.fn>;
  let getQueueTransfers: ReturnType<typeof vi.fn>;
  let setTransferProperties: ReturnType<typeof vi.fn>;
  let service: QueueActionsService;

  beforeEach(() => {
    createQueue = vi.fn().mockReturnValue(of(undefined));
    setQueueProperties = vi.fn().mockReturnValue(of(undefined));
    refreshNow = vi.fn();
    deleteQueue = vi.fn().mockReturnValue(of(undefined));
    confirm = vi.fn();
    transfersRefresh = vi.fn();
    messagesAdd = vi.fn();
    getQueueTransfers = vi.fn().mockReturnValue(
      of(['Completed', 'Active', 'Waiting', 'Paused', 'Failed', 'ForcedActive'].map((state) => ({ uuid: state.toLowerCase(), state }))),
    );
    setTransferProperties = vi.fn().mockReturnValue(of(undefined));

    TestBed.configureTestingModule({
      providers: [
        QueueActionsService,
        { provide: TransferApi, useValue: { createQueue, setQueueProperties, deleteQueue, getQueueTransfers, setTransferProperties } },
        { provide: MessageService, useValue: { add: messagesAdd } },
        { provide: ConfirmationService, useValue: { confirm } },
        { provide: TransfersStore, useValue: { refreshNow: transfersRefresh } },
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

  it('shows the server message when saving a queue fails', () => {
    setQueueProperties.mockReturnValue(throwError(() => new ApiError(422, 'watch_directory_in_use', 'The queue "TV" already watches /srv/w')));
    service.updateQueueProperties('q1', { watchDirectory: '/srv/w' });
    expect(messagesAdd).toHaveBeenCalledWith(expect.objectContaining({ severity: 'error', summary: 'Cannot save the queue', detail: 'The queue "TV" already watches /srv/w' }));
  });

  it('falls back to the message of a plain Error', () => {
    setQueueProperties.mockReturnValue(throwError(() => new Error('boom')));
    service.updateQueueProperties('q1', { name: 'x' });
    expect(messagesAdd).toHaveBeenCalledWith(expect.objectContaining({ detail: 'boom' }));
  });

  it('shows the server message when creating a queue fails', () => {
    createQueue.mockReturnValue(throwError(() => new ApiError(422, 'invalid_watch_directory', 'Not a directory')));
    service.createQueue({ name: 'TV', watchDirectory: '/x' });
    expect(messagesAdd).toHaveBeenCalledWith(expect.objectContaining({ severity: 'error', detail: 'Not a directory' }));
    expect(refreshNow).not.toHaveBeenCalled();
  });

  it('confirmDeleteEmptyQueue() asks for confirmation and deletes only once accepted', () => {
    service.confirmDeleteEmptyQueue({ uuid: 'q1', name: 'Movies' } as never);
    expect(deleteQueue).not.toHaveBeenCalled();
    expect(confirm.mock.calls[0][0].message).toContain('Movies');

    confirm.mock.calls[0][0].accept();
    expect(deleteQueue).toHaveBeenCalledWith('q1', {});
    expect(refreshNow).toHaveBeenCalled();
    expect(transfersRefresh).toHaveBeenCalled();
  });

  it('deleteQueue() passes the options through and refreshes', () => {
    service.deleteQueue({ uuid: 'q1', name: 'Movies' } as never, { moveTo: 'q2' });
    expect(deleteQueue).toHaveBeenCalledWith('q1', { moveTo: 'q2' });
    expect(refreshNow).toHaveBeenCalled();
    expect(transfersRefresh).toHaveBeenCalled();
  });

  it('resumeAll() queues only the paused and failed transfers, like the desktop', () => {
    service.resumeAll('q1');
    expect(getQueueTransfers).toHaveBeenCalledWith('q1');
    expect(setTransferProperties).toHaveBeenCalledTimes(1);
    expect(setTransferProperties).toHaveBeenCalledWith(['paused', 'failed'], { state: 'Waiting' });
  });

  it('stopAll() pauses everything that is not completed', () => {
    service.stopAll('q1');
    expect(setTransferProperties).toHaveBeenCalledWith(['active', 'waiting', 'paused', 'failed', 'forcedactive'], { state: 'Paused' });
  });

  it('resumeAll() sends nothing when no transfer qualifies', () => {
    getQueueTransfers.mockReturnValue(of([{ uuid: 'a', state: 'Completed' }]));
    service.resumeAll('q1');
    expect(setTransferProperties).not.toHaveBeenCalled();
  });

  it('createQueue() does not refresh when the API call errors', () => {
    createQueue.mockReturnValue(throwError(() => new Error('boom')));
    service.createQueue({ name: 'Movies' });
    expect(refreshNow).not.toHaveBeenCalled();
  });
});
