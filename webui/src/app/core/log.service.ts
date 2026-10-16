import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

@Injectable({ providedIn: 'root' })
export class LogService {
  private readonly http = inject(HttpClient);

  getGlobalLog(): Observable<string> {
    return this.http.get('/log', { responseType: 'text' });
  }

  getTransferLog(transferUuid: string): Observable<string> {
    return this.http.get(`/log/${transferUuid}`, { responseType: 'text' });
  }
}
