import { Observable } from 'rxjs';
import {
  Queue,
  Transfer,
  TransferClass,
  SettingsPage,
  SpeedSample,
  SpeedLimits,
  NewTransferOptions,
} from './models';
import { JsonValue } from './json-value';

/**
 * Domain-shaped access to the FatRat remote-control backend. The concrete
 * transport (REST/JSON) is hidden behind this abstraction so it can be
 * swapped later without touching any consumer of TransferApi.
 */
export abstract class TransferApi {
  abstract getTransferClasses(): Observable<TransferClass[]>;

  abstract getQueues(): Observable<Queue[]>;
  abstract createQueue(properties: Partial<Queue>): Observable<void>;
  abstract setQueueProperties(uuid: string, properties: Partial<Queue>): Observable<void>;
  abstract getQueueTransfers(queueUuid: string): Observable<Transfer[]>;
  abstract getQueueSpeedGraph(queueUuid: string): Observable<SpeedSample[]>;
  abstract moveTransfers(
    queueUuid: string,
    transferUuids: string[],
    direction: 'top' | 'up' | 'down' | 'bottom',
  ): Observable<void>;

  abstract getTransferProperties(uuid: string): Observable<Transfer>;
  abstract getTransferAdvancedProperties(uuid: string): Observable<{ [key: string]: JsonValue }>;
  abstract setTransferProperties(
    uuids: string[],
    properties: { [key: string]: JsonValue },
  ): Observable<void>;
  abstract deleteTransfers(uuids: string[], withData: boolean): Observable<void>;
  abstract getTransferSpeedGraph(uuid: string): Observable<SpeedSample[]>;

  abstract addTransfers(urls: string[], options: NewTransferOptions): Observable<string[]>;
  abstract addTransferWithData(file: File, options: NewTransferOptions): Observable<string>;

  abstract getSettingsValues(keys: string[]): Observable<JsonValue[]>;
  abstract setSettingsValues(keys: string[], values: JsonValue[]): Observable<void>;
  abstract applySettings(): Observable<void>;
  abstract getSettingsPages(): Observable<SettingsPage[]>;
}
