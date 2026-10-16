import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { TransferApi } from './transfer-api';
import { RestClient } from './rest-client';
import { JsonValue } from './json-value';
import {
  DirectoryListing,
  NewTransferOptions,
  DeleteQueueOptions,
  Queue,
  SettingsPage,
  SpeedSample,
  Stats,
  Transfer,
  TransferClass,
} from './models';

@Injectable({ providedIn: 'root' })
export class RestTransferApi extends TransferApi {
  private readonly rest = inject(RestClient);

  getTransferClasses(): Observable<TransferClass[]> {
    return this.rest.get<TransferClass[]>('/transfer-classes');
  }

  browseDirectories(path?: string, queueUuid?: string, showHidden?: boolean): Observable<DirectoryListing> {
    const query: Record<string, string | boolean> = {};
    if (path) query['path'] = path;
    if (queueUuid) query['queue'] = queueUuid;
    if (showHidden) query['showHidden'] = true;
    return this.rest.get<DirectoryListing>('/filesystem/directories', query);
  }

  getStats(): Observable<Stats> {
    return this.rest.get<Stats>('/stats');
  }

  getQueues(): Observable<Queue[]> {
    return this.rest.get<Queue[]>('/queues');
  }

  createQueue(properties: Partial<Queue>): Observable<void> {
    return this.rest.post<Queue>('/queues', properties).pipe(map(() => undefined));
  }

  setQueueProperties(uuid: string, properties: Partial<Queue>): Observable<void> {
    return this.rest
      .patch<Queue>(`/queues/${encodeURIComponent(uuid)}`, properties)
      .pipe(map(() => undefined));
  }

  deleteQueue(uuid: string, options: DeleteQueueOptions = {}): Observable<void> {
    const query: Record<string, string | boolean> = {};
    if (options.moveTo) query['moveTo'] = options.moveTo;
    if (options.deleteTransfers) query['deleteTransfers'] = true;
    if (options.withData) query['withData'] = true;
    return this.rest.delete<void>(`/queues/${encodeURIComponent(uuid)}`, query).pipe(map(() => undefined));
  }

  getQueueTransfers(queueUuid: string): Observable<Transfer[]> {
    return this.rest.get<Transfer[]>(`/queues/${encodeURIComponent(queueUuid)}/transfers`);
  }

  getQueueSpeedGraph(queueUuid: string): Observable<SpeedSample[]> {
    return this.rest.get<SpeedSample[]>(`/queues/${encodeURIComponent(queueUuid)}/speed-data`);
  }

  moveTransfers(
    queueUuid: string,
    transferUuids: string[],
    direction: 'top' | 'up' | 'down' | 'bottom',
  ): Observable<void> {
    return this.rest
      .post<void>(`/queues/${encodeURIComponent(queueUuid)}/transfers/move`, {
        uuids: transferUuids,
        direction,
      })
      .pipe(map(() => undefined));
  }

  getTransferProperties(uuid: string): Observable<Transfer> {
    return this.rest.get<Transfer>(`/transfers/${encodeURIComponent(uuid)}`);
  }

  getTransferAdvancedProperties(uuid: string): Observable<{ [key: string]: JsonValue }> {
    return this.rest.get<{ [key: string]: JsonValue }>(
      `/transfers/${encodeURIComponent(uuid)}/details`,
    );
  }

  setTransferProperties(
    uuids: string[],
    properties: { [key: string]: JsonValue },
  ): Observable<void> {
    return this.rest.patch<void>('/transfers', { uuids, properties }).pipe(map(() => undefined));
  }

  deleteTransfers(uuids: string[], withData: boolean): Observable<void> {
    return this.rest.delete<void>('/transfers', { uuid: uuids, withData }).pipe(map(() => undefined));
  }

  getTransferSpeedGraph(uuid: string): Observable<SpeedSample[]> {
    return this.rest.get<SpeedSample[]>(`/transfers/${encodeURIComponent(uuid)}/speed-data`);
  }

  addTransfers(urls: string[], options: NewTransferOptions): Observable<string[]> {
    return this.rest
      .post<{ uuids: string[] }>(`/queues/${encodeURIComponent(options.queueUuid)}/transfers`, {
        urls,
        ...toAddOptions(options),
      })
      .pipe(map((res) => res.uuids));
  }

  addTransferWithData(file: File, options: NewTransferOptions): Observable<string> {
    const form = new FormData();
    form.append('file', file, file.name);
    form.append('options', JSON.stringify(toAddOptions(options)));
    return this.rest
      .post<{ uuids: string[] }>(`/queues/${encodeURIComponent(options.queueUuid)}/transfers`, form)
      .pipe(map((res) => res.uuids[0]));
  }

  getSettingsValues(keys: string[]): Observable<JsonValue[]> {
    return this.rest
      .get<{ [key: string]: JsonValue }>('/settings', { key: keys })
      .pipe(map((res) => keys.map((key) => res[key] ?? null)));
  }

  setSettingsValues(keys: string[], values: JsonValue[]): Observable<void> {
    const body: { [key: string]: JsonValue } = {};
    keys.forEach((key, i) => (body[key] = values[i]));
    return this.rest.patch<void>('/settings', body).pipe(map(() => undefined));
  }

  applySettings(): Observable<void> {
    return this.rest.post<void>('/settings/apply').pipe(map(() => undefined));
  }

  getSettingsPages(): Observable<SettingsPage[]> {
    return this.rest.get<SettingsPage[]>('/settings/pages');
  }
}

function toAddOptions(options: NewTransferOptions) {
  return {
    upload: options.upload,
    class: options.targetClass,
    target: options.target,
    paused: options.paused,
    speedLimits: { down: options.speedLimits.down, up: options.speedLimits.up },
  };
}
