import { Type } from '@angular/core';
import { TorrentDownloadDetails } from './torrent-download-details';

/**
 * Keyed by Transfer.detailsScript (the exact path the backend returns from
 * TorrentDownload::detailsScript(), e.g. "/scripts/transfers/TorrentDownload.js").
 * Transfer classes with no entry render no subclass panel, matching legacy behavior.
 *
 * Every entry is rendered via NgComponentOutlet with `inputs: { transfer: transfer }`
 * (see transfer-details.html), so every component here must declare
 * `@Input({ required: true }) transfer!: Transfer`. TypeScript can't enforce this
 * structurally through `Type<unknown>` — NgComponentOutlet sets inputs dynamically at
 * runtime, so a component without a matching `transfer` input would fail silently in
 * production (a dev-only console.error) or hit a confusing required-input error instead
 * of a clear type error at this file.
 */
export const TRANSFER_DETAIL_COMPONENTS: Record<string, Type<unknown>> = {
  '/scripts/transfers/TorrentDownload.js': TorrentDownloadDetails,
};
