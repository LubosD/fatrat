import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { TransferDetails } from './transfer-details';
import { TransfersStore } from '../../state/transfers.store';
import { BrowseService } from '../../core/browse.service';
import { Transfer } from '../../core/api/models';

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

function configure(selectedUuids: string[], transfers: Transfer[]) {
  const browseList = vi.fn().mockReturnValue(of([]));
  TestBed.configureTestingModule({
    imports: [TransferDetails],
    providers: [
      { provide: TransfersStore, useValue: { transfers: () => transfers, selectedUuids: () => selectedUuids } },
      { provide: BrowseService, useValue: { list: browseList } },
    ],
  });
  return { browseList };
}

describe('TransferDetails', () => {
  it('shows a placeholder message when nothing is selected', () => {
    configure([], []);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    expect(fixture.nativeElement.textContent).toContain('Select a single transfer to view details');
  });

  it('shows a placeholder message when multiple transfers are selected', () => {
    configure(['t1', 't2'], [makeTransfer({ uuid: 't1' }), makeTransfer({ uuid: 't2' })]);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    expect(fixture.nativeElement.textContent).toContain('Select a single transfer to view details');
  });

  it('shows a direct download link for a single non-directory transfer', () => {
    configure(['t1'], [makeTransfer({ uuid: 't1', name: 'file.zip', dataPathIsDir: false })]);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();

    const link: HTMLAnchorElement = fixture.nativeElement.querySelector('a[data-testid="download-link"]');
    expect(link).toBeTruthy();
    expect(link.getAttribute('href')).toBe('/download?transfer=t1');
    expect(link.textContent).toContain('file.zip');
  });
});
