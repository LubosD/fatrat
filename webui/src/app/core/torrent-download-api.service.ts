import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { RestClient } from './api/rest-client';

@Injectable({ providedIn: 'root' })
export class TorrentDownloadApiService {
  private readonly rest = inject(RestClient);

  /** priorities maps a 0-based file index to a priority (0=do not download, 1=normal, 4=increased, 7=maximum). */
  setFilePriorities(transferUuid: string, priorities: Record<number, number>): Observable<void> {
    const body = Object.entries(priorities).map(([index, priority]) => ({
      index: Number(index),
      priority,
    }));
    return this.rest
      .put<void>(`/transfers/${encodeURIComponent(transferUuid)}/torrent/file-priorities`, body)
      .pipe(map(() => undefined));
  }
}
