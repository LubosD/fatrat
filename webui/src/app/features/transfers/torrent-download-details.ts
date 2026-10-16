import { Component, Input, OnChanges, OnDestroy, SimpleChanges, inject, signal } from '@angular/core';
import { Subscription } from 'rxjs';
import { TableModule } from 'primeng/table';
import { ProgressBar } from 'primeng/progressbar';
import { Select } from 'primeng/select';
import { FormsModule } from '@angular/forms';
import { TransferApi } from '../../core/api/transfer-api';
import { TorrentDownloadApiService } from '../../core/torrent-download-api.service';
import { PollingService } from '../../core/polling.service';
import { Transfer } from '../../core/api/models';
import { JsonValue } from '../../core/api/json-value';
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

function toTorrentDownloadProperties(raw: { [key: string]: JsonValue }): TorrentDownloadProperties {
  const rawFiles = Array.isArray(raw['files']) ? raw['files'] : [];
  return {
    totalDownload: typeof raw['totalDownload'] === 'string' ? raw['totalDownload'] : '',
    totalUpload: typeof raw['totalUpload'] === 'string' ? raw['totalUpload'] : '',
    comment: typeof raw['comment'] === 'string' ? raw['comment'] : '',
    magnetLink: typeof raw['magnetLink'] === 'string' ? raw['magnetLink'] : '',
    ratio: typeof raw['ratio'] === 'string' ? raw['ratio'] : '',
    files: rawFiles.map((f) => {
      const file = f as { [key: string]: JsonValue };
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
  imports: [TableModule, ProgressBar, Select, FormsModule],
  styleUrl: './torrent-download-details.scss',
})
export class TorrentDownloadDetails implements OnChanges, OnDestroy {
  @Input({ required: true }) transfer!: Transfer;

  private readonly transferApi = inject(TransferApi);
  protected readonly torrentApi = inject(TorrentDownloadApiService);
  private readonly polling = inject(PollingService);

  private readonly pollSubscription: Subscription;

  constructor() {
    this.pollSubscription = this.polling.ticks$.subscribe(() => {
      this.refreshImages();
      this.fetchProperties();
    });
  }

  ngOnDestroy(): void {
    this.pollSubscription.unsubscribe();
  }

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
    const requestUuid = this.transfer.uuid;
    this.transferApi.getTransferAdvancedProperties(requestUuid).subscribe({
      next: (raw) => {
        // Guard against a stale response: only apply it if this is still the selected transfer.
        if (this.transfer.uuid !== requestUuid) {
          return;
        }
        this.properties.set(toTorrentDownloadProperties(raw));
      },
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

  protected readonly priorityOptions = [
    { label: 'Do not download', value: 0 },
    { label: 'Normal', value: 1 },
    { label: 'Increased', value: 4 },
    { label: 'Maximum', value: 7 },
  ];

  protected fileName(path: string): string {
    const slash = path.indexOf('/');
    return slash === -1 ? path : `[...]${path.slice(slash)}`;
  }

  protected fileProgress(file: TorrentDownloadFile): number {
    return file.size > 0 ? Math.min(100, Math.round((100 * file.done) / file.size)) : 0;
  }

  protected readonly trackByFileName = (_: number, file: TorrentDownloadFile): string => file.name;

  protected onPriorityChange(fileIndex: number, priority: number): void {
    const current = this.properties();
    if (!current) {
      return;
    }
    const requestUuid = this.transfer.uuid;
    const fileName = current.files[fileIndex]?.name;
    this.torrentApi.setFilePriorities(requestUuid, { [fileIndex]: priority }).subscribe({
      next: () => {
        // Guard against a stale response: only apply it if this is still the selected transfer.
        if (this.transfer.uuid !== requestUuid) {
          return;
        }
        this.properties.update((props) =>
          props
            ? { ...props, files: props.files.map((f) => (f.name === fileName ? { ...f, priority } : f)) }
            : props,
        );
      },
      error: (err) => console.error('Failed to set file priority', err),
    });
  }
}
