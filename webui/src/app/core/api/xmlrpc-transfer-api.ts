import { Injectable } from '@angular/core';
import { Observable, Subscription } from 'rxjs';
import { map } from 'rxjs/operators';
import { TransferApi } from './transfer-api';
import { XmlRpcClient } from '../xmlrpc/xmlrpc-client';
import { XmlRpcBinary, XmlRpcValue } from '../xmlrpc/xmlrpc-value';
import {
  NewTransferOptions,
  Queue,
  SettingsPage,
  SpeedLimits,
  SpeedSample,
  Transfer,
  TransferClass,
} from './models';

function toSpeedLimits(pair: XmlRpcValue): SpeedLimits {
  const [down, up] = pair as number[];
  return { down, up };
}

function toQueue(raw: { [key: string]: XmlRpcValue }): Queue {
  return {
    uuid: raw['uuid'] as string,
    name: raw['name'] as string,
    defaultDirectory: raw['defaultDirectory'] as string,
    moveDirectory: raw['moveDirectory'] as string,
    upAsDown: raw['upAsDown'] as boolean,
    transferLimits: toSpeedLimits(raw['transferLimits']),
    speedLimits: toSpeedLimits(raw['speedLimits']),
  };
}

function toTransfer(raw: { [key: string]: XmlRpcValue }): Transfer {
  return {
    uuid: raw['uuid'] as string,
    name: raw['name'] as string,
    state: raw['state'] as Transfer['state'],
    class: raw['class'] as string,
    message: raw['message'] as string,
    mode: raw['mode'] as Transfer['mode'],
    primaryMode: raw['primaryMode'] as Transfer['primaryMode'],
    dataPath: raw['dataPath'] as string,
    dataPathIsDir: raw['dataPathIsDir'] as boolean,
    total: raw['total'] as number,
    done: raw['done'] as number,
    comment: raw['comment'] as string,
    object: raw['object'] as string,
    timeRunning: raw['timeRunning'] as number,
    speeds: toSpeedLimits(raw['speeds']),
    userSpeedLimits: toSpeedLimits(raw['userSpeedLimits']),
    detailsScript: raw['detailsScript'] as string | undefined,
  };
}

// Backend (XmlRpcService::Queue_create/Queue_setProperties) requires speedLimits/transferLimits
// as a 2-element [down, up] array, not a {down, up} struct — convert only the fields present.
function toQueuePropertiesPayload(properties: Partial<Queue>): { [key: string]: XmlRpcValue } {
  const payload: { [key: string]: XmlRpcValue } = {
    ...properties,
  } as unknown as { [key: string]: XmlRpcValue };
  if (properties.speedLimits) {
    payload['speedLimits'] = [properties.speedLimits.down, properties.speedLimits.up];
  }
  if (properties.transferLimits) {
    payload['transferLimits'] = [properties.transferLimits.down, properties.transferLimits.up];
  }
  return payload;
}

function toTransferClass(raw: { [key: string]: XmlRpcValue }): TransferClass {
  return {
    mode: raw['mode'] as TransferClass['mode'],
    shortName: raw['shortName'] as string,
    longName: raw['longName'] as string,
  };
}

function toSettingsPage(raw: { [key: string]: XmlRpcValue }): SettingsPage {
  return {
    title: raw['title'] as string,
    script: raw['script'] as string,
    icon: raw['icon'] as string,
  };
}

/** Parses the "down,up;down,up;" format produced by XmlRpcService::speedDataToString. */
function parseSpeedGraph(raw: XmlRpcValue): SpeedSample[] {
  const text = raw as string;
  if (text.length === 0) return [];
  return text
    .split(';')
    .filter((entry) => entry.length > 0)
    .map((entry) => {
      const [down, up] = entry.split(',').map(Number);
      return { down, up };
    });
}

@Injectable({ providedIn: 'root' })
export class XmlRpcTransferApi extends TransferApi {
  constructor(private readonly client: XmlRpcClient) {
    super();
  }

  getTransferClasses(): Observable<TransferClass[]> {
    return this.client
      .call('getTransferClasses')
      .pipe(map((raw) => (raw as { [key: string]: XmlRpcValue }[]).map(toTransferClass)));
  }

  getQueues(): Observable<Queue[]> {
    return this.client
      .call('getQueues')
      .pipe(map((raw) => (raw as { [key: string]: XmlRpcValue }[]).map(toQueue)));
  }

  createQueue(properties: Partial<Queue>): Observable<void> {
    return this.client
      .call('Queue_create', toQueuePropertiesPayload(properties))
      .pipe(map(() => undefined));
  }

  setQueueProperties(uuid: string, properties: Partial<Queue>): Observable<void> {
    return this.client
      .call('Queue_setProperties', uuid, toQueuePropertiesPayload(properties))
      .pipe(map(() => undefined));
  }

  getQueueTransfers(queueUuid: string): Observable<Transfer[]> {
    return this.client
      .call('Queue_getTransfers', queueUuid)
      .pipe(map((raw) => (raw as { [key: string]: XmlRpcValue }[]).map(toTransfer)));
  }

  getQueueSpeedGraph(queueUuid: string): Observable<SpeedSample[]> {
    return this.client.call('Queue_getSpeedGraph', queueUuid).pipe(map(parseSpeedGraph));
  }

  moveTransfers(
    queueUuid: string,
    transferUuids: string[],
    direction: 'top' | 'up' | 'down' | 'bottom',
  ): Observable<void> {
    return this.client
      .call('Queue_moveTransfers', queueUuid, transferUuids, direction)
      .pipe(map(() => undefined));
  }

  getTransferProperties(uuid: string): Observable<Transfer> {
    return this.client
      .call('Transfer_getProperties', uuid)
      .pipe(map((raw) => toTransfer(raw as { [key: string]: XmlRpcValue })));
  }

  getTransferAdvancedProperties(uuid: string): Observable<{ [key: string]: XmlRpcValue }> {
    return this.client
      .call('Transfer_getAdvancedProperties', uuid)
      .pipe(map((raw) => raw as { [key: string]: XmlRpcValue }));
  }

  setTransferProperties(
    uuids: string[],
    properties: { [key: string]: XmlRpcValue },
  ): Observable<void> {
    return this.client.call('Transfer_setProperties', uuids, properties).pipe(map(() => undefined));
  }

  deleteTransfers(uuids: string[], withData: boolean): Observable<void> {
    return this.client.call('Transfer_delete', uuids, withData).pipe(map(() => undefined));
  }

  getTransferSpeedGraph(uuid: string): Observable<SpeedSample[]> {
    return this.client.call('Transfer_getSpeedGraph', uuid).pipe(map(parseSpeedGraph));
  }

  addTransfers(urls: string[], options: NewTransferOptions): Observable<string[]> {
    return this.client
      .call(
        'Queue_addTransfers',
        options.upload,
        options.queueUuid,
        urls,
        options.targetClass,
        options.target,
        options.paused,
        options.speedLimits.down,
        options.speedLimits.up,
      )
      .pipe(map((raw) => raw as string[]));
  }

  addTransferWithData(file: File, options: NewTransferOptions): Observable<string> {
    return new Observable<string>((subscriber) => {
      let cancelled = false;
      let sub: Subscription | undefined;
      XmlRpcBinary.fromFile(file).then(
        (data) => {
          if (cancelled) return;
          sub = this.client
            .call(
              'Queue_addTransferWithData',
              options.upload,
              options.queueUuid,
              file.name,
              data,
              options.targetClass,
              options.target,
              options.paused,
              options.speedLimits.down,
              options.speedLimits.up,
            )
            .pipe(map((raw) => raw as string))
            .subscribe(subscriber);
        },
        (err) => subscriber.error(err),
      );
      return () => {
        cancelled = true;
        sub?.unsubscribe();
      };
    });
  }

  getSettingsValues(keys: string[]): Observable<XmlRpcValue[]> {
    return this.client.call('Settings_getValue', keys).pipe(map((raw) => raw as XmlRpcValue[]));
  }

  setSettingsValues(keys: string[], values: XmlRpcValue[]): Observable<void> {
    return this.client.call('Settings_setValue', keys, values).pipe(map(() => undefined));
  }

  applySettings(): Observable<void> {
    return this.client.call('Settings_apply').pipe(map(() => undefined));
  }

  getSettingsPages(): Observable<SettingsPage[]> {
    return this.client
      .call('Settings_getPages')
      .pipe(map((raw) => (raw as { [key: string]: XmlRpcValue }[]).map(toSettingsPage)));
  }

  callRaw(method: string, ...params: XmlRpcValue[]): Observable<XmlRpcValue> {
    return this.client.call(method, ...params);
  }
}
