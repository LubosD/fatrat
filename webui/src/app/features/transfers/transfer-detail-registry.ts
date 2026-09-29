import { Type } from '@angular/core';
import { TorrentDownloadDetailsPlaceholder } from './torrent-download-details-placeholder';

/**
 * Keyed by Transfer.detailsScript (the exact path the backend returns from
 * TorrentDownload::detailsScript(), e.g. "/scripts/transfers/TorrentDownload.js").
 * Transfer classes with no entry render no subclass panel, matching legacy behavior.
 */
export const TRANSFER_DETAIL_COMPONENTS: Record<string, Type<unknown>> = {
  '/scripts/transfers/TorrentDownload.js': TorrentDownloadDetailsPlaceholder,
};
