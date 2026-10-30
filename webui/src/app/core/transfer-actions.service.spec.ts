import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { ConfirmationService } from '@openng/optimus-ui/api';
import { TransferActionsService } from './transfer-actions.service';
import { TransferApi } from './api/transfer-api';
import { TransfersStore } from '../state/transfers.store';
import { QueuesStore } from '../state/queues.store';
import { Transfer } from './api/models';

function makeTransfer(uuid: string, state: Transfer['state']): Transfer {
  return {
    uuid,
    name: uuid,
    state,
    class: 'HttpDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/' + uuid,
    dataPathIsDir: false,
    total: 100,
    done: 0,
    comment: '',
    completionCommand: '',
    object: '',
    timeRunning: 0,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
  };
}

describe('TransferActionsService', () => {
  let setTransferProperties: ReturnType<typeof vi.fn>;
  let deleteTransfers: ReturnType<typeof vi.fn>;
  let moveTransfers: ReturnType<typeof vi.fn>;
  let moveTransfersBefore: ReturnType<typeof vi.fn>;
  let addTransfers: ReturnType<typeof vi.fn>;
  let addTransferWithData: ReturnType<typeof vi.fn>;
  let confirm: ReturnType<typeof vi.fn>;
  let clearSelection: ReturnType<typeof vi.fn>;
  let refreshNow: ReturnType<typeof vi.fn>;
  let selectedUuids: string[];
  let transfers: Transfer[];
  let selectedQueueUuid: string | null;
  let service: TransferActionsService;

  beforeEach(() => {
    setTransferProperties = vi.fn().mockReturnValue(of(undefined));
    deleteTransfers = vi.fn().mockReturnValue(of(undefined));
    moveTransfers = vi.fn().mockReturnValue(of(undefined));
    moveTransfersBefore = vi.fn().mockReturnValue(of(undefined));
    addTransfers = vi.fn().mockReturnValue(of(['new-uuid']));
    addTransferWithData = vi.fn().mockReturnValue(of('new-uuid'));
    confirm = vi.fn();
    clearSelection = vi.fn();
    refreshNow = vi.fn();
    selectedUuids = ['t1', 't2'];
    transfers = [makeTransfer('t1', 'Completed'), makeTransfer('t2', 'Active')];
    selectedQueueUuid = 'q1';

    TestBed.configureTestingModule({
      providers: [
        TransferActionsService,
        { provide: TransferApi, useValue: { setTransferProperties, deleteTransfers, moveTransfers, moveTransfersBefore, addTransfers, addTransferWithData } },
        {
          provide: TransfersStore,
          useValue: {
            selectedUuids: () => selectedUuids,
            transfers: () => transfers,
            clearSelection,
            refreshNow,
          },
        },
        { provide: QueuesStore, useValue: { selectedQueueUuid: () => selectedQueueUuid } },
        { provide: ConfirmationService, useValue: { confirm } },
      ],
    });
    service = TestBed.inject(TransferActionsService);
  });

  it('resume() queues paused and failed transfers, turns forced ones into Active, and skips the rest', () => {
    transfers = [
      makeTransfer('t1', 'Completed'),
      makeTransfer('t2', 'Active'),
      makeTransfer('t3', 'Paused'),
      makeTransfer('t4', 'ForcedActive'),
      makeTransfer('t5', 'Failed'),
      makeTransfer('t6', 'Waiting'),
      makeTransfer('t7', 'Paused'),
    ];
    selectedUuids = ['t1', 't2', 't3', 't4', 't5', 't6'];
    service.resume();
    expect(setTransferProperties).toHaveBeenCalledTimes(2);
    expect(setTransferProperties).toHaveBeenCalledWith(['t4'], { state: 'Active' });
    expect(setTransferProperties).toHaveBeenCalledWith(['t3', 't5'], { state: 'Waiting' });
    expect(refreshNow).toHaveBeenCalled();
  });

  it('resume() sends nothing when no selected transfer can be resumed', () => {
    service.resume();
    expect(setTransferProperties).not.toHaveBeenCalled();
  });

  it('forceResume() sets state to ForcedActive', () => {
    service.forceResume();
    expect(setTransferProperties).toHaveBeenCalledWith(['t1', 't2'], { state: 'ForcedActive' });
  });

  it('pause() sets state to Paused', () => {
    service.pause();
    expect(setTransferProperties).toHaveBeenCalledWith(['t1', 't2'], { state: 'Paused' });
  });

  it('resume()/forceResume()/pause() do nothing when nothing is selected', () => {
    selectedUuids = [];
    service.resume();
    service.forceResume();
    service.pause();
    expect(setTransferProperties).not.toHaveBeenCalled();
  });

  it('deleteSelected() asks for confirmation before deleting anything', () => {
    service.deleteSelected(false);
    expect(confirm).toHaveBeenCalledTimes(1);
    expect(deleteTransfers).not.toHaveBeenCalled();
  });

  it('deleteSelected() deletes, clears selection, and refreshes once the user accepts', () => {
    confirm.mockImplementation((c: { accept: () => void }) => c.accept());
    service.deleteSelected(false);
    expect(deleteTransfers).toHaveBeenCalledWith(['t1', 't2'], false);
    expect(clearSelection).toHaveBeenCalled();
    expect(refreshNow).toHaveBeenCalled();
  });

  it('deleteSelected(true) passes withData through once accepted', () => {
    confirm.mockImplementation((c: { accept: () => void }) => c.accept());
    service.deleteSelected(true);
    expect(deleteTransfers).toHaveBeenCalledWith(['t1', 't2'], true);
  });

  it('deleteSelected() does nothing (not even asking for confirmation) when nothing is selected', () => {
    selectedUuids = [];
    service.deleteSelected(false);
    expect(confirm).not.toHaveBeenCalled();
    expect(deleteTransfers).not.toHaveBeenCalled();
  });

  it('deleteCompleted() asks for confirmation naming only the Completed transfers', () => {
    service.deleteCompleted();
    expect(confirm).toHaveBeenCalledTimes(1);
    expect(deleteTransfers).not.toHaveBeenCalled();
  });

  it('deleteCompleted() deletes only Completed-state transfers once accepted', () => {
    confirm.mockImplementation((c: { accept: () => void }) => c.accept());
    service.deleteCompleted();
    expect(deleteTransfers).toHaveBeenCalledWith(['t1'], false);
    expect(refreshNow).toHaveBeenCalled();
  });

  it('deleteCompleted() does nothing when no transfer is in the Completed state', () => {
    transfers = [makeTransfer('t1', 'Active')];
    service.deleteCompleted();
    expect(confirm).not.toHaveBeenCalled();
  });

  it('moveSelected() moves the selected transfers in the given direction for the selected queue', () => {
    service.moveSelected('top');
    expect(moveTransfers).toHaveBeenCalledWith('q1', ['t1', 't2'], 'top');
    expect(refreshNow).toHaveBeenCalled();
  });

  it('moveSelected() does nothing when no queue is selected', () => {
    selectedQueueUuid = null;
    service.moveSelected('up');
    expect(moveTransfers).not.toHaveBeenCalled();
  });

  it('moveSelected() does nothing when nothing is selected', () => {
    selectedUuids = [];
    service.moveSelected('down');
    expect(moveTransfers).not.toHaveBeenCalled();
  });

  it('moveBefore() moves the given transfers before another one in the selected queue, then refreshes', () => {
    service.moveBefore(['t3'], 't1');
    expect(moveTransfersBefore).toHaveBeenCalledWith('q1', ['t3'], 't1');
    expect(refreshNow).toHaveBeenCalled();
  });

  it('moveBefore() does nothing when no queue is selected', () => {
    selectedQueueUuid = null;
    service.moveBefore(['t3'], null);
    expect(moveTransfersBefore).not.toHaveBeenCalled();
  });

  it('updateProperties() PATCHes the selected transfers, then refreshes', () => {
    let done = false;
    service.updateProperties({ comment: 'new comment' }).subscribe(() => (done = true));
    expect(setTransferProperties).toHaveBeenCalledWith(['t1', 't2'], { comment: 'new comment' });
    expect(refreshNow).toHaveBeenCalled();
    expect(done).toBe(true);
  });

  it('updateProperties() passes errors on and does not refresh', () => {
    setTransferProperties.mockReturnValue(throwError(() => new Error('nope')));
    let error: unknown;
    service.updateProperties({ comment: 'x' }).subscribe({ error: (e) => (error = e) });
    expect((error as Error).message).toBe('nope');
    expect(refreshNow).not.toHaveBeenCalled();
  });

  it('updateProperties() completes without a call when nothing is selected', () => {
    selectedUuids = [];
    let done = false;
    service.updateProperties({ comment: 'x' }).subscribe(() => (done = true));
    expect(setTransferProperties).not.toHaveBeenCalled();
    expect(done).toBe(true);
  });

  it('refresh() reloads the transfer list', () => {
    service.refresh();
    expect(refreshNow).toHaveBeenCalled();
  });

  const addOptions = { upload: false, queueUuid: 'q1', targetClass: '', target: '/downloads', paused: false, speedLimits: { down: 0, up: 0 } };

  it('addTransfers() calls TransferApi.addTransfers() and refreshes', () => {
    service.addTransfers(['http://example.com/a'], addOptions).subscribe();
    expect(addTransfers).toHaveBeenCalledWith(['http://example.com/a'], addOptions);
    expect(refreshNow).toHaveBeenCalled();
  });

  it('addTransfers() does nothing when the url list is empty', () => {
    let completed = false;
    service.addTransfers([], addOptions).subscribe({ complete: () => (completed = true) });
    expect(addTransfers).not.toHaveBeenCalled();
    expect(completed).toBe(true);
  });

  it('addTransfers() passes the error on and does not refresh', () => {
    addTransfers.mockReturnValue(throwError(() => new Error('nope')));
    let message = '';
    service.addTransfers(['magnet:?xt=a'], addOptions).subscribe({ error: (e: Error) => (message = e.message) });
    expect(message).toBe('nope');
    expect(refreshNow).not.toHaveBeenCalled();
  });

  it('addTransferWithData() calls TransferApi.addTransferWithData() and refreshes', () => {
    const file = new File(['data'], 'test.torrent');
    service.addTransferWithData(file, addOptions).subscribe();
    expect(addTransferWithData).toHaveBeenCalledWith(file, addOptions);
    expect(refreshNow).toHaveBeenCalled();
  });
});
