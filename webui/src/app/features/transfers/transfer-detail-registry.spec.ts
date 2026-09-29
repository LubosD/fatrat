import { TRANSFER_DETAIL_COMPONENTS } from './transfer-detail-registry';
import { TorrentDownloadDetailsPlaceholder } from './torrent-download-details-placeholder';

describe('TRANSFER_DETAIL_COMPONENTS', () => {
  it('maps the TorrentDownload detailsScript path to its placeholder component', () => {
    expect(TRANSFER_DETAIL_COMPONENTS['/scripts/transfers/TorrentDownload.js']).toBe(TorrentDownloadDetailsPlaceholder);
  });

  it('has no entry for an unknown detailsScript', () => {
    expect(TRANSFER_DETAIL_COMPONENTS['/scripts/transfers/Nonexistent.js']).toBeUndefined();
  });
});
