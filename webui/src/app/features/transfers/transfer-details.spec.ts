import { signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { of, Subject, NEVER } from 'rxjs';
import { TransferDetails } from './transfer-details';
import { TransfersStore } from '../../state/transfers.store';
import { BrowseService } from '../../core/browse.service';
import { TransferApi } from '../../core/api/transfer-api';
import { TorrentDownloadApiService } from '../../core/torrent-download-api.service';
import { PollingService } from '../../core/polling.service';
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
    completionCommand: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

function configure(selectedUuids: string[], transfers: Transfer[]) {
  const browseList = vi.fn().mockReturnValue(of([]));
  const transfersSignal = signal<Transfer[]>(transfers);
  const selectedUuidsSignal = signal<string[]>(selectedUuids);
  TestBed.configureTestingModule({
    imports: [TransferDetails],
    providers: [
      { provide: TransfersStore, useValue: { transfers: transfersSignal, selectedUuids: selectedUuidsSignal } },
      { provide: BrowseService, useValue: { list: browseList } },
      { provide: TransferApi, useValue: { getTransferAdvancedProperties: () => of({}) } },
      { provide: TorrentDownloadApiService, useValue: { setFilePriorities: () => of(undefined) } },
      { provide: PollingService, useValue: { ticks$: NEVER } },
    ],
  });
  return { browseList, transfersSignal, selectedUuidsSignal };
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

  it('refetches the browse tree when the selected transfer uuid changes', async () => {
    const { browseList, selectedUuidsSignal } = configure(
      ['t1'],
      [makeTransfer({ uuid: 't1', dataPathIsDir: true }), makeTransfer({ uuid: 't2', dataPathIsDir: true })],
    );
    browseList.mockReturnValue(of([{ key: '/movies', label: 'movies', leaf: false }]));

    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(browseList).toHaveBeenCalledWith('t1', '');
    expect(browseList).toHaveBeenCalledTimes(1);

    browseList.mockReturnValue(of([{ key: '/shows', label: 'shows', leaf: false }]));
    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(browseList).toHaveBeenCalledWith('t2', '');
    expect(browseList).toHaveBeenCalledTimes(2);
  });

  it('does not refetch the browse tree when transfers() updates but the selected transfer is unchanged (Bug 1)', async () => {
    const { browseList, transfersSignal } = configure(['t1'], [makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    browseList.mockReturnValue(of([{ key: '/movies', label: 'movies', leaf: false }]));

    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(browseList).toHaveBeenCalledTimes(1);

    // Simulate a poll tick: TransfersStore.refresh() sets a brand-new array of brand-new Transfer
    // objects with the same primitive field values — nothing about the selection actually changed.
    transfersSignal.set([makeTransfer({ uuid: 't1', dataPathIsDir: true })]);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(browseList).toHaveBeenCalledTimes(1);
  });

  it('ignores a stale browse.list response from a previously-selected transfer (Bug 2)', async () => {
    const { browseList, selectedUuidsSignal } = configure(
      ['t1'],
      [makeTransfer({ uuid: 't1', dataPathIsDir: true }), makeTransfer({ uuid: 't2', dataPathIsDir: true })],
    );
    const t1Subject = new Subject<{ key: string; label: string; leaf: boolean }[]>();
    const t2Subject = new Subject<{ key: string; label: string; leaf: boolean }[]>();
    browseList.mockReturnValueOnce(t1Subject);

    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(browseList).toHaveBeenCalledWith('t1', '');

    browseList.mockReturnValueOnce(t2Subject);
    selectedUuidsSignal.set(['t2']);
    fixture.detectChanges();
    await fixture.whenStable();

    expect(browseList).toHaveBeenCalledWith('t2', '');

    // t2's response arrives first and renders correctly.
    t2Subject.next([{ key: '/shows', label: 'shows', leaf: false }]);
    expect(fixture.componentInstance['treeNodes']()).toEqual([{ key: '/shows', label: 'shows', leaf: false }]);

    // t1's stale response arrives late — it must not clobber the currently-displayed t2 data.
    t1Subject.next([{ key: '/movies', label: 'movies', leaf: false }]);
    expect(fixture.componentInstance['treeNodes']()).toEqual([{ key: '/shows', label: 'shows', leaf: false }]);
  });

  it('renders the registered subclass panel when detailsScript matches', () => {
    configure(['t1'], [makeTransfer({ uuid: 't1', detailsScript: '/scripts/transfers/TorrentDownload.js' })]);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('app-torrent-download-details')).toBeTruthy();
  });

  it('renders no subclass panel when detailsScript has no registry entry', () => {
    configure(['t1'], [makeTransfer({ uuid: 't1', detailsScript: undefined })]);
    const fixture = TestBed.createComponent(TransferDetails);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('app-torrent-download-details')).toBeNull();
  });
});
