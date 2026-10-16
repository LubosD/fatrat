import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { RestClient } from './api/rest-client';

@Injectable({ providedIn: 'root' })
export class WebinterfaceSettingsApiService {
  private readonly rest = inject(RestClient);

  /** Returns the path of the generated certificate file. */
  generateCertificate(hostname: string): Observable<string> {
    return this.rest
      .post<{ path: string }>('/settings/web-interface/certificate', { hostname })
      .pipe(map((res) => res.path));
  }
}
