import { TRANSFER_DETAIL_COMPONENTS } from './transfer-detail-registry';
import { TorrentDownloadDetails } from './torrent-download-details';

describe('TRANSFER_DETAIL_COMPONENTS', () => {
  it('maps the TorrentDownload detailsScript path to TorrentDownloadDetails', () => {
    expect(TRANSFER_DETAIL_COMPONENTS['/scripts/transfers/TorrentDownload.js']).toBe(TorrentDownloadDetails);
  });

  it('has no entry for an unknown detailsScript', () => {
    expect(TRANSFER_DETAIL_COMPONENTS['/scripts/transfers/Nonexistent.js']).toBeUndefined();
  });
});
