import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { XmlRpcClient } from './xmlrpc/xmlrpc-client';

@Injectable({ providedIn: 'root' })
export class TorrentDownloadApiService {
  private readonly client = inject(XmlRpcClient);

  /** priorities maps a 0-based file index to a priority (0=do not download, 1=normal, 4=increased, 7=maximum). */
  setFilePriorities(transferUuid: string, priorities: Record<number, number>): Observable<void> {
    return this.client
      .call('TorrentDownload.setFilePriorities', transferUuid, priorities)
      .pipe(map(() => undefined));
  }
}
