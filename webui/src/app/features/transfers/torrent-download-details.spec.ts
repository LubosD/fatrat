import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { NEVER } from 'rxjs';
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

function configure(getTransferAdvancedProperties = vi.fn().mockReturnValue(of({}))) {
  TestBed.configureTestingModule({
    imports: [TorrentDownloadDetails],
    providers: [
      { provide: TransferApi, useValue: { getTransferAdvancedProperties } },
      { provide: TorrentDownloadApiService, useValue: { setFilePriorities: vi.fn().mockReturnValue(of(undefined)) } },
      { provide: PollingService, useValue: { ticks$: NEVER } },
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
});
