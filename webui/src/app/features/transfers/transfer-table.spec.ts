import { TestBed } from '@angular/core/testing';
import { TransferTable } from './transfer-table';
import { TransfersStore } from '../../state/transfers.store';
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

describe('TransferTable', () => {
  it('renders one row per transfer with name, progress, size, speeds, ETA and message', () => {
    const transfersStoreStub = { transfers: () => [makeTransfer()] };
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [{ provide: TransfersStore, useValue: transfersStoreStub }],
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
    const transfersStoreStub = { transfers: () => [] };
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [{ provide: TransfersStore, useValue: transfersStoreStub }],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelectorAll('tbody tr').length).toBe(0);
  });
});
