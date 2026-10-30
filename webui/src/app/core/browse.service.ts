import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import type { TreeNode } from '@openng/optimus-ui/api';

@Injectable({ providedIn: 'root' })
export class BrowseService {
  private readonly http = inject(HttpClient);

  /** Lists the contents of `path` inside `transferUuid`'s data directory. Pass '' for the root. */
  list(transferUuid: string, path: string): Observable<TreeNode[]> {
    return this.http
      .get(`/browse/${transferUuid}`, { params: { path }, responseType: 'text' })
      .pipe(map(parseBrowseXml));
  }
}

function parseBrowseXml(xmlText: string): TreeNode[] {
  const doc = new DOMParser().parseFromString(xmlText, 'text/xml');
  return Array.from(doc.getElementsByTagName('item')).map((item) => {
    const id = item.getAttribute('id') ?? '';
    const type = item.getAttribute('type') ?? 'default';
    const name = item.getElementsByTagName('name')[0]?.textContent ?? id;
    return { key: id, label: name, leaf: type !== 'folder' };
  });
}
