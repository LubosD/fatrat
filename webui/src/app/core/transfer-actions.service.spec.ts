import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { ConfirmationService } from 'primeng/api';
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
        { provide: TransferApi, useValue: { setTransferProperties, deleteTransfers, moveTransfers, addTransfers, addTransferWithData } },
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

  it('resume() sets state to Waiting for the selected transfers and refreshes', () => {
    service.resume();
    expect(setTransferProperties).toHaveBeenCalledWith(['t1', 't2'], { state: 'Waiting' });
    expect(refreshNow).toHaveBeenCalled();
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

  it('updateProperties() sets comment and userSpeedLimits as a [down, up] array, then refreshes', () => {
    service.updateProperties('new comment', { down: 100, up: 50 });
    expect(setTransferProperties).toHaveBeenCalledWith(['t1', 't2'], { comment: 'new comment', userSpeedLimits: [100, 50] });
    expect(refreshNow).toHaveBeenCalled();
  });

  it('updateProperties() includes the new data directory as "object" when given', () => {
    service.updateProperties('c', { down: 0, up: 0 }, '/new/dir');
    expect(setTransferProperties).toHaveBeenCalledWith(['t1', 't2'], { comment: 'c', userSpeedLimits: [0, 0], object: '/new/dir' });
  });

  it('updateProperties() does nothing when nothing is selected', () => {
    selectedUuids = [];
    service.updateProperties('x', { down: 0, up: 0 });
    expect(setTransferProperties).not.toHaveBeenCalled();
  });

  it('addTransfers() calls TransferApi.addTransfers() and refreshes', () => {
    const options = { upload: false, queueUuid: 'q1', targetClass: '', target: '/downloads', paused: false, speedLimits: { down: 0, up: 0 } };
    service.addTransfers(['http://example.com/a'], options);
    expect(addTransfers).toHaveBeenCalledWith(['http://example.com/a'], options);
    expect(refreshNow).toHaveBeenCalled();
  });

  it('addTransfers() does nothing when the url list is empty', () => {
    service.addTransfers([], { upload: false, queueUuid: 'q1', targetClass: '', target: '', paused: false, speedLimits: { down: 0, up: 0 } });
    expect(addTransfers).not.toHaveBeenCalled();
  });

  it('addTransferWithData() calls TransferApi.addTransferWithData() and refreshes', () => {
    const file = new File(['data'], 'test.torrent');
    const options = { upload: false, queueUuid: 'q1', targetClass: '', target: '/downloads', paused: false, speedLimits: { down: 0, up: 0 } };
    service.addTransferWithData(file, options);
    expect(addTransferWithData).toHaveBeenCalledWith(file, options);
    expect(refreshNow).toHaveBeenCalled();
  });
});
