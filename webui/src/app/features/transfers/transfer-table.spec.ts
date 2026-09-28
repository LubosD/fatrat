import { TestBed } from '@angular/core/testing';
import { TransferTable } from './transfer-table';
import { TransfersStore } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer } from '../../core/api/models';

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'file.zip',
    state: 'Active',
    class: 'HttpDownload',
    message: 'Downloading',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/file.zip',
    dataPathIsDir: false,
    total: 1000,
    done: 250,
    comment: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 2048, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

function makeTransfersStoreStub(transfers: Transfer[], selected: string[] = []) {
  return {
    transfers: () => transfers,
    isSelected: (uuid: string) => selected.includes(uuid),
    toggleSelection: vi.fn(),
    selectOnly: vi.fn(),
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
  };
}

describe('TransferTable', () => {
  it('renders one row per transfer with name, progress, size, speeds, ETA and message', () => {
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([makeTransfer()]) },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    const text = fixture.nativeElement.textContent as string;
    expect(text).toContain('file.zip');
    expect(text).toContain('25%');
    expect(text).toContain('1000 B');
    expect(text).toContain('2.0 KB/s');
    expect(text).toContain('Downloading');
  });

  it('renders an empty table when there are no transfers', () => {
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([]) },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelectorAll('tbody tr').length).toBe(0);
  });

  it('trackByUuid returns the transfer uuid for row identity tracking', () => {
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([]) },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    const transfer = makeTransfer();
    expect(fixture.componentInstance['trackByUuid'](0, transfer)).toBe('t1');
  });

  it('renders a checkbox per row reflecting the selection state from the store', () => {
    const store = makeTransfersStoreStub(
      [makeTransfer({ uuid: 't1' }), makeTransfer({ uuid: 't2', name: 'other.zip' })],
      ['t2'],
    );
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: store },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    const checkboxes = fixture.nativeElement.querySelectorAll('input[type="checkbox"]');
    expect(checkboxes.length).toBe(2);
    expect((checkboxes[0] as HTMLInputElement).checked).toBe(false);
    expect((checkboxes[1] as HTMLInputElement).checked).toBe(true);
  });

  it('toggling a checkbox calls TransfersStore.toggleSelection() with the transfer uuid', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })]);
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: store },
        { provide: TransferActionsService, useValue: makeActionsStub() },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    const checkbox = fixture.nativeElement.querySelector('input[type="checkbox"]');
    checkbox.dispatchEvent(new Event('change'));
    expect(store.toggleSelection).toHaveBeenCalledWith('t1');
  });
});
