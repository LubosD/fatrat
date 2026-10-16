import { Observable } from 'rxjs';
import {
  DeleteQueueOptions,
  Queue,
  Transfer,
  TransferClass,
  SettingsPage,
  DirectoryListing,
  SpeedSample,
  SpeedLimits,
  Stats,
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

  /**
   * Lists the sub-directories of a directory on the server. With no path the listing starts at the
   * queue's default directory, or the server user's home directory.
   */
  abstract browseDirectories(
    path?: string,
    queueUuid?: string,
    showHidden?: boolean,
  ): Observable<DirectoryListing>;

  abstract getStats(): Observable<Stats>;
  abstract getQueues(): Observable<Queue[]>;
  abstract createQueue(properties: Partial<Queue>): Observable<void>;
  abstract setQueueProperties(uuid: string, properties: Partial<Queue>): Observable<void>;
  abstract deleteQueue(uuid: string, options?: DeleteQueueOptions): Observable<void>;
  abstract getQueueTransfers(queueUuid: string): Observable<Transfer[]>;
  abstract getQueueSpeedGraph(queueUuid: string): Observable<SpeedSample[]>;
  abstract moveTransfers(
    queueUuid: string,
    transferUuids: string[],
    direction: 'top' | 'up' | 'down' | 'bottom',
  ): Observable<void>;
  /** Moves the transfers in front of `before`, or to the end of the queue when `before` is null. */
  abstract moveTransfersBefore(
    queueUuid: string,
    transferUuids: string[],
    before: string | null,
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
