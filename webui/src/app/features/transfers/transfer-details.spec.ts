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

  it('loads and renders the root of the browse tree for a directory transfer', async () => {
    const { browseList } = configure(['t1'], [makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    browseList.mockReturnValue(of([{ key: '/movies', label: 'movies', leaf: false }]));

    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(browseList).toHaveBeenCalledWith('t1', '');
    expect(fixture.nativeElement.textContent).toContain('movies');
  });

  it('does not render a download link for a directory transfer', async () => {
    const { browseList } = configure(['t1'], [makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    browseList.mockReturnValue(of([]));

    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('a[data-testid="download-link"]')).toBeNull();
  });

  it('fetches children lazily on node expand and merges them into the tree', async () => {
    const { browseList } = configure(['t1'], [makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    browseList.mockReturnValue(of([{ key: '/movies', label: 'movies', leaf: false }]));

    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    await fixture.whenStable();

    browseList.mockReturnValue(of([{ key: '/movies/one.mkv', label: 'one.mkv', leaf: true }]));
    fixture.componentInstance['onNodeExpand']({
      originalEvent: new Event('click'),
      node: fixture.componentInstance['treeNodes']()[0],
    });

    expect(browseList).toHaveBeenCalledWith('t1', '/movies');
    expect(fixture.componentInstance['treeNodes']()[0].children).toEqual([{ key: '/movies/one.mkv', label: 'one.mkv', leaf: true }]);
  });

  it('navigates to the download URL when a leaf node is selected', () => {
    configure(['t1'], [makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    const navigateTo = vi.spyOn(fixture.componentInstance as any, 'navigateTo');

    fixture.componentInstance['onNodeSelect']({
      originalEvent: new Event('click'),
      node: { key: '/readme.txt', label: 'readme.txt', leaf: true },
    });
    expect(navigateTo).toHaveBeenCalledWith('/download?transfer=t1&path=%2Freadme.txt');
  });

  it('does nothing when a folder node is selected', () => {
    configure(['t1'], [makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    const navigateTo = vi.spyOn(fixture.componentInstance as any, 'navigateTo');

    fixture.componentInstance['onNodeSelect']({
      originalEvent: new Event('click'),
      node: { key: '/movies', label: 'movies', leaf: false },
    });
    expect(navigateTo).not.toHaveBeenCalled();
  });
});
