import { Component, Input, OnChanges, SimpleChanges, inject, signal } from '@angular/core';
import { TransferApi } from '../../core/api/transfer-api';
import { TorrentDownloadApiService } from '../../core/torrent-download-api.service';
import { PollingService } from '../../core/polling.service';
import { Transfer } from '../../core/api/models';
import { XmlRpcValue } from '../../core/xmlrpc/xmlrpc-value';
import { formatSize, formatSpeed } from './format';

interface TorrentDownloadFile {
  name: string;
  size: number;
  done: number;
  priority: number;
}

interface TorrentDownloadProperties {
  totalDownload: string;
  totalUpload: string;
  comment: string;
  magnetLink: string;
  ratio: string;
  files: TorrentDownloadFile[];
}

function toTorrentDownloadProperties(raw: { [key: string]: XmlRpcValue }): TorrentDownloadProperties {
  const rawFiles = (raw['files'] as XmlRpcValue[] | undefined) ?? [];
  return {
    totalDownload: typeof raw['totalDownload'] === 'string' ? raw['totalDownload'] : '',
    totalUpload: typeof raw['totalUpload'] === 'string' ? raw['totalUpload'] : '',
    comment: typeof raw['comment'] === 'string' ? raw['comment'] : '',
    magnetLink: typeof raw['magnetLink'] === 'string' ? raw['magnetLink'] : '',
    ratio: typeof raw['ratio'] === 'string' ? raw['ratio'] : '',
    files: rawFiles.map((f) => {
      const file = f as { [key: string]: XmlRpcValue };
      return {
        name: typeof file['name'] === 'string' ? file['name'] : '',
        size: typeof file['size'] === 'number' ? file['size'] : 0,
        done: typeof file['done'] === 'number' ? file['done'] : 0,
        priority: typeof file['priority'] === 'number' ? file['priority'] : 1,
      };
    }),
  };
}

@Component({
  selector: 'app-torrent-download-details',
  templateUrl: './torrent-download-details.html',
})
export class TorrentDownloadDetails implements OnChanges {
  @Input({ required: true }) transfer!: Transfer;

  private readonly transferApi = inject(TransferApi);
  protected readonly torrentApi = inject(TorrentDownloadApiService);
  private readonly polling = inject(PollingService);

  protected readonly formatSize = formatSize;
  protected readonly formatSpeed = formatSpeed;

  protected readonly properties = signal<TorrentDownloadProperties | null>(null);

  protected readonly progressImageUrl = signal('');
  protected readonly availabilityImageUrl = signal('');

  ngOnChanges(changes: SimpleChanges): void {
    if ('transfer' in changes) {
      this.refreshImages();
      this.fetchProperties();
    }
  }

  protected progressText(): string {
    const t = this.transfer;
    const percent = t.total > 0 ? ((100 * t.done) / t.total).toFixed(1) : '0.0';
    return t.mode === 'Download' ? `${percent}%, ${this.formatSize(t.total - t.done)} left` : `${percent}%`;
  }

  protected commentLines(): string[] {
    const comment = this.properties()?.comment ?? '';
    return comment.split('<br>');
  }

  private fetchProperties(): void {
    this.transferApi.getTransferAdvancedProperties(this.transfer.uuid).subscribe({
      next: (raw) => this.properties.set(toTorrentDownloadProperties(raw)),
      error: (err) => console.error('Failed to fetch torrent details', err),
    });
  }

  // An incrementing counter, not Date.now(): two refreshes in the same millisecond (routine in
  // synchronous test code) would otherwise produce identical "cache-busted" URLs.
  private cacheBusterCounter = 0;

  private refreshImages(): void {
    this.cacheBusterCounter++;
    const uuid = encodeURIComponent(this.transfer.uuid);
    this.progressImageUrl.set(`/subclass/progress?transfer=${uuid}&xx=${this.cacheBusterCounter}`);
    this.availabilityImageUrl.set(`/subclass/availability?transfer=${uuid}&xx=${this.cacheBusterCounter}`);
  }
}
