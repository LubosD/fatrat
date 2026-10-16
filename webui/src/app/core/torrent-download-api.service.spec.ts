import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { TorrentDownloadApiService } from './torrent-download-api.service';

describe('TorrentDownloadApiService', () => {
  it('PUTs the priorities as [{index, priority}] to the torrent file-priorities endpoint', () => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    const service = TestBed.inject(TorrentDownloadApiService);
    const httpMock = TestBed.inject(HttpTestingController);
    let result: unknown = 'unset';

    service.setFilePriorities('t1', { 3: 7, 0: 0 }).subscribe((v) => (result = v));

    const req = httpMock.expectOne('/api/v1/transfers/t1/torrent/file-priorities');
    expect(req.request.method).toBe('PUT');
    expect(req.request.body).toEqual([
      { index: 0, priority: 0 },
      { index: 3, priority: 7 },
    ]);
    req.flush(null, { status: 204, statusText: 'No Content' });
    expect(result).toBeUndefined();
    httpMock.verify();
  });
});
