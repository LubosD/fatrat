import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { TorrentDownloadApiService } from './torrent-download-api.service';
import { XmlRpcClient } from './xmlrpc/xmlrpc-client';

describe('TorrentDownloadApiService', () => {
  it('calls TorrentDownload.setFilePriorities with the transfer uuid and priorities map', () => {
    return new Promise<void>((done) => {
      const call = vi.fn().mockReturnValue(of(null));
      TestBed.configureTestingModule({
        providers: [TorrentDownloadApiService, { provide: XmlRpcClient, useValue: { call } }],
      });
      const service = TestBed.inject(TorrentDownloadApiService);

      service.setFilePriorities('t1', { 3: 7 }).subscribe(() => {
        expect(call).toHaveBeenCalledWith('TorrentDownload.setFilePriorities', 't1', { 3: 7 });
        done();
      });
    });
  });
});
