import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { XmlRpcClient } from './xmlrpc/xmlrpc-client';

@Injectable({ providedIn: 'root' })
export class WebinterfaceSettingsApiService {
  private readonly client = inject(XmlRpcClient);

  generateCertificate(hostname: string): Observable<string> {
    return this.client.call('HttpService.generateCertificate', hostname).pipe(map((raw) => raw as string));
  }
}
