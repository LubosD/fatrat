import { TestBed } from '@angular/core/testing';
import { NEVER, Observable, of, Subject } from 'rxjs';
import { TorrentDownloadDetails } from './torrent-download-details';
import { TransferApi } from '../../core/api/transfer-api';
import { TorrentDownloadApiService } from '../../core/torrent-download-api.service';
import { PollingService } from '../../core/polling.service';
import { Transfer } from '../../core/api/models';

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'some.torrent',
    state: 'Active',
    class: 'TorrentDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/some',
    dataPathIsDir: true,
    total: 1000,
    done: 250,
    comment: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 100, up: 50 },
    userSpeedLimits: { down: 0, up: 0 },
    detailsScript: '/scripts/transfers/TorrentDownload.js',
    ...overrides,
  };
}

function configure(
  getTransferAdvancedProperties = vi.fn().mockReturnValue(of({})),
  ticks$: Observable<number> = NEVER,
) {
  TestBed.configureTestingModule({
    imports: [TorrentDownloadDetails],
    providers: [
      { provide: TransferApi, useValue: { getTransferAdvancedProperties } },
      { provide: TorrentDownloadApiService, useValue: { setFilePriorities: vi.fn().mockReturnValue(of(undefined)) } },
      { provide: PollingService, useValue: { ticks$ } },
    ],
  });
  return { getTransferAdvancedProperties };
}

describe('TorrentDownloadDetails', () => {
  it('fetches advanced properties for the given transfer on init', () => {
    const { getTransferAdvancedProperties } = configure();
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();

    expect(getTransferAdvancedProperties).toHaveBeenCalledWith('t1');
  });

  it('renders speed/progress text from the Transfer input, not the advanced-properties response', () => {
    configure();
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer({ speeds: { down: 2048, up: 1024 }, total: 1000, done: 250 }));
    fixture.detectChanges();

    const text = fixture.nativeElement.textContent;
    expect(text).toContain('2.0 KB/s');
    expect(text).toContain('1.0 KB/s');
  });

  it('renders totals, ratio and magnet link from the advanced-properties response', () => {
    configure(
      vi.fn().mockReturnValue(
        of({
          totalDownload: '100 MB',
          totalUpload: '50 MB',
          ratio: '0.5',
          magnetLink: 'magnet:?xt=urn:btih:abc',
          comment: '',
          files: [],
        }),
      ),
    );
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();

    const text = fixture.nativeElement.textContent;
    expect(text).toContain('100 MB');
    expect(text).toContain('50 MB');
    expect(text).toContain('0.5');
    const magnetInput: HTMLInputElement = fixture.nativeElement.querySelector('input[data-testid="magnet-link"]');
    expect(magnetInput.value).toBe('magnet:?xt=urn:btih:abc');
  });

  it('renders a multi-line comment as separate lines, without interpreting it as HTML', () => {
    configure(vi.fn().mockReturnValue(of({ comment: 'line one<br>line two', files: [] })));
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();

    const commentEl: HTMLElement = fixture.nativeElement.querySelector('[data-testid="comment"]');
    // Two separate text-bearing children (e.g. two <p> or <div> lines), not one element containing a literal "<br>" tag.
    expect(commentEl.textContent).toContain('line one');
    expect(commentEl.textContent).toContain('line two');
    expect(commentEl.innerHTML).not.toContain('&lt;br&gt;');
    expect(commentEl.querySelectorAll('br').length).toBe(0); // no raw <br> tag was ever parsed as markup
  });

  it('tolerates an empty advanced-properties response (torrent metadata not yet loaded)', () => {
    configure(vi.fn().mockReturnValue(of({})));
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    expect(() => fixture.detectChanges()).not.toThrow();
  });

  it('refetches when the transfer input changes to a different uuid', () => {
    const { getTransferAdvancedProperties } = configure();
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't1' }));
    fixture.detectChanges();
    fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't2' }));
    fixture.detectChanges();

    expect(getTransferAdvancedProperties).toHaveBeenNthCalledWith(1, 't1');
    expect(getTransferAdvancedProperties).toHaveBeenNthCalledWith(2, 't2');
  });

  it('renders a row per file with size, progress and priority', () => {
    configure(
      vi.fn().mockReturnValue(
        of({
          files: [
            { name: 'movie/video.mkv', size: 1000, done: 500, priority: 1 },
            { name: 'movie/subs.srt', size: 100, done: 100, priority: 4 },
          ],
        }),
      ),
    );
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();

    const rows = fixture.nativeElement.querySelectorAll('[data-testid="file-row"]');
    expect(rows.length).toBe(2);
    expect(rows[0].textContent).toContain('video.mkv');
    expect(rows[1].textContent).toContain('subs.srt');
  });

  it('calls TorrentDownloadApiService.setFilePriorities and optimistically updates local state on priority change', () => {
    const setFilePriorities = vi.fn().mockReturnValue(of(undefined));
    TestBed.configureTestingModule({
      imports: [TorrentDownloadDetails],
      providers: [
        {
          provide: TransferApi,
          useValue: {
            getTransferAdvancedProperties: vi
              .fn()
              .mockReturnValue(of({ files: [{ name: 'a.txt', size: 10, done: 0, priority: 1 }] })),
          },
        },
        { provide: TorrentDownloadApiService, useValue: { setFilePriorities } },
        { provide: PollingService, useValue: { ticks$: NEVER } },
      ],
    });
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();

    fixture.componentInstance['onPriorityChange'](0, 7);

    expect(setFilePriorities).toHaveBeenCalledWith('t1', { 0: 7 });
    expect(fixture.componentInstance['properties']()?.files[0].priority).toBe(7);
  });

  it('does not apply a stale setFilePriorities response after the transfer input has switched to a different torrent', () => {
    const priorityResponse$ = new Subject<void>();
    const setFilePriorities = vi.fn().mockReturnValue(priorityResponse$);
    const getTransferAdvancedProperties = vi.fn().mockImplementation((uuid: string) =>
      uuid === 't1'
        ? of({ files: [{ name: 't1-file.txt', size: 10, done: 0, priority: 1 }] })
        : of({ files: [{ name: 't2-file.txt', size: 20, done: 0, priority: 1 }] }),
    );
    TestBed.configureTestingModule({
      imports: [TorrentDownloadDetails],
      providers: [
        { provide: TransferApi, useValue: { getTransferAdvancedProperties } },
        { provide: TorrentDownloadApiService, useValue: { setFilePriorities } },
        { provide: PollingService, useValue: { ticks$: NEVER } },
      ],
    });
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't1' }));
    fixture.detectChanges();

    // Trigger a priority change on t1, but don't resolve it yet.
    fixture.componentInstance['onPriorityChange'](0, 7);
    expect(setFilePriorities).toHaveBeenCalledWith('t1', { 0: 7 });

    // Switch to a different torrent (t2) before t1's response arrives.
    fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't2' }));
    fixture.detectChanges();
    expect(fixture.componentInstance['properties']()?.files[0].name).toBe('t2-file.txt');

    // Now resolve t1's stale response.
    priorityResponse$.next();
    priorityResponse$.complete();

    // t2's file must not have been mutated by t1's stale response.
    expect(fixture.componentInstance['properties']()?.files[0].name).toBe('t2-file.txt');
    expect(fixture.componentInstance['properties']()?.files[0].priority).toBe(1);
  });

  it('does not apply a stale fetchProperties response after the transfer input has switched to a different torrent', () => {
    const t1Response$ = new Subject<{ [key: string]: unknown }>();
    const getTransferAdvancedProperties = vi.fn().mockImplementation((uuid: string) =>
      uuid === 't1' ? t1Response$ : of({ totalDownload: 't2-total', ratio: 't2-ratio', comment: '', files: [] }),
    );
    TestBed.configureTestingModule({
      imports: [TorrentDownloadDetails],
      providers: [
        { provide: TransferApi, useValue: { getTransferAdvancedProperties } },
        { provide: TorrentDownloadApiService, useValue: { setFilePriorities: vi.fn().mockReturnValue(of(undefined)) } },
        { provide: PollingService, useValue: { ticks$: NEVER } },
      ],
    });
    const fixture = TestBed.createComponent(TorrentDownloadDetails);

    // Select transfer A (t1). Its fetchProperties() fires via ngOnChanges but doesn't resolve yet.
    fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't1' }));
    fixture.detectChanges();
    expect(getTransferAdvancedProperties).toHaveBeenCalledWith('t1');

    // Switch to a different torrent (t2) before t1's response arrives. t2's own fetchProperties()
    // resolves synchronously via `of(...)`.
    fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't2' }));
    fixture.detectChanges();
    expect(fixture.componentInstance['properties']()?.totalDownload).toBe('t2-total');

    // Now resolve t1's now-stale response.
    t1Response$.next({ totalDownload: 't1-total', ratio: 't1-ratio', comment: '', files: [] });
    t1Response$.complete();

    // properties() must still reflect t2's data, not t1's stale response.
    expect(fixture.componentInstance['properties']()?.totalDownload).toBe('t2-total');
    expect(fixture.componentInstance['properties']()?.ratio).toBe('t2-ratio');
  });

  it('refetches properties and bumps the image cache-buster on every poll tick', () => {
    const ticks$ = new Subject<number>();
    const { getTransferAdvancedProperties } = configure(vi.fn().mockReturnValue(of({})), ticks$);
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();
    expect(getTransferAdvancedProperties).toHaveBeenCalledTimes(1);
    const firstImageUrl = fixture.componentInstance['progressImageUrl']();

    ticks$.next(0);

    expect(getTransferAdvancedProperties).toHaveBeenCalledTimes(2);
    expect(fixture.componentInstance['progressImageUrl']()).not.toBe(firstImageUrl);
  });

  it('unsubscribes from poll ticks on destroy', () => {
    const ticks$ = new Subject<number>();
    const { getTransferAdvancedProperties } = configure(vi.fn().mockReturnValue(of({})), ticks$);
    const fixture = TestBed.createComponent(TorrentDownloadDetails);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.detectChanges();
    expect(getTransferAdvancedProperties).toHaveBeenCalledTimes(1);

    fixture.destroy();
    ticks$.next(0);

    expect(getTransferAdvancedProperties).toHaveBeenCalledTimes(1);
  });
});
