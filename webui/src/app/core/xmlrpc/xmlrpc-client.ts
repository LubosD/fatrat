import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { encodeMethodCall, decodeMethodResponse } from './xmlrpc-codec';
import { XmlRpcValue } from './xmlrpc-value';

@Injectable({ providedIn: 'root' })
export class XmlRpcClient {
  constructor(private readonly http: HttpClient) {}

  call(method: string, ...params: XmlRpcValue[]): Observable<XmlRpcValue> {
    const body = encodeMethodCall(method, params);
    return this.http
      .post('/xmlrpc', body, { headers: { 'Content-Type': 'text/xml' }, responseType: 'text' })
      .pipe(map((xmlText) => decodeMethodResponse(xmlText)));
  }
}
