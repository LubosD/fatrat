import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { RestTransferApi } from './rest-transfer-api';
import { NewTransferOptions } from './models';

const OPTIONS: NewTransferOptions = {
  upload: false,
  queueUuid: 'q1',
  targetClass: 'HttpDownload',
  target: '/downloads',
  paused: true,
  speedLimits: { down: 10, up: 20 },
};
const WIRE_OPTIONS = {
  upload: false,
  class: 'HttpDownload',
  target: '/downloads',
  paused: true,
  speedLimits: { down: 10, up: 20 },
};
const NO_CONTENT = { status: 204, statusText: 'No Content' };

describe('RestTransferApi', () => {
  let api: RestTransferApi;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    api = TestBed.inject(RestTransferApi);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('getTransferClasses: GET /transfer-classes', () => {
    let out: unknown;
    api.getTransferClasses().subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/transfer-classes');
    expect(req.request.method).toBe('GET');
    const body = [{ mode: 'Download', shortName: 'HttpDownload', longName: 'HTTP' }];
    req.flush(body);
    expect(out).toEqual(body);
  });

  it('getQueues: GET /queues', () => {
    let out: unknown;
    api.getQueues().subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/queues');
    expect(req.request.method).toBe('GET');
    const body = [
      {
        uuid: 'q1',
        name: 'Default',
        defaultDirectory: '/d',
        moveDirectory: '',
        upAsDown: false,
        transferLimits: { down: 0, up: 0 },
        speedLimits: { down: 1, up: 2 },
      },
    ];
    req.flush(body);
    expect(out).toEqual(body);
  });

  it('createQueue: POST /queues with the properties as-is', () => {
    let done = false;
    const props = { name: 'N', speedLimits: { down: 1, up: 2 } };
    api.createQueue(props).subscribe(() => (done = true));
    const req = httpMock.expectOne('/api/v1/queues');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual(props);
    req.flush({ uuid: 'new' }, { status: 201, statusText: 'Created' });
    expect(done).toBe(true);
  });

  it('setQueueProperties: PATCH /queues/{uuid}', () => {
    let done = false;
    api.setQueueProperties('q1', { upAsDown: true }).subscribe(() => (done = true));
    const req = httpMock.expectOne('/api/v1/queues/q1');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ upAsDown: true });
    req.flush({});
    expect(done).toBe(true);
  });

  it('getQueueTransfers: GET /queues/{uuid}/transfers', () => {
    let out: unknown;
    api.getQueueTransfers('q1').subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/queues/q1/transfers');
    expect(req.request.method).toBe('GET');
    req.flush([]);
    expect(out).toEqual([]);
  });

  it('getQueueSpeedGraph: GET /queues/{uuid}/speed-data, JSON array passed through', () => {
    let out: unknown;
    api.getQueueSpeedGraph('q1').subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/queues/q1/speed-data');
    expect(req.request.method).toBe('GET');
    req.flush([{ down: 1, up: 2 }]);
    expect(out).toEqual([{ down: 1, up: 2 }]);
  });

  it('moveTransfers: POST /queues/{uuid}/transfers/move {uuids, direction}', () => {
    let done = false;
    api.moveTransfers('q1', ['a', 'b'], 'top').subscribe(() => (done = true));
    const req = httpMock.expectOne('/api/v1/queues/q1/transfers/move');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ uuids: ['a', 'b'], direction: 'top' });
    req.flush(null, NO_CONTENT);
    expect(done).toBe(true);
  });

  it('getTransferProperties: GET /transfers/{uuid}', () => {
    let out: unknown;
    api.getTransferProperties('t1').subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/transfers/t1');
    expect(req.request.method).toBe('GET');
    req.flush({ uuid: 't1' });
    expect(out).toEqual({ uuid: 't1' });
  });

  it('getTransferAdvancedProperties: GET /transfers/{uuid}/details', () => {
    let out: unknown;
    api.getTransferAdvancedProperties('t1').subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/transfers/t1/details');
    expect(req.request.method).toBe('GET');
    req.flush({ ratio: '1.0' });
    expect(out).toEqual({ ratio: '1.0' });
  });

  it('setTransferProperties: PATCH /transfers {uuids, properties}', () => {
    let done = false;
    api.setTransferProperties(['a', 'b'], { state: 'Paused' }).subscribe(() => (done = true));
    const req = httpMock.expectOne('/api/v1/transfers');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ uuids: ['a', 'b'], properties: { state: 'Paused' } });
    req.flush(null, NO_CONTENT);
    expect(done).toBe(true);
  });

  it('deleteTransfers: DELETE /transfers?uuid=a&uuid=b&withData=true', () => {
    let done = false;
    api.deleteTransfers(['a', 'b'], true).subscribe(() => (done = true));
    const req = httpMock.expectOne((r) => r.url === '/api/v1/transfers');
    expect(req.request.method).toBe('DELETE');
    expect(req.request.params.getAll('uuid')).toEqual(['a', 'b']);
    expect(req.request.params.get('withData')).toBe('true');
    req.flush(null, NO_CONTENT);
    expect(done).toBe(true);
  });

  it('getTransferSpeedGraph: GET /transfers/{uuid}/speed-data', () => {
    let out: unknown;
    api.getTransferSpeedGraph('t1').subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/transfers/t1/speed-data');
    expect(req.request.method).toBe('GET');
    req.flush([{ down: 3, up: 4 }]);
    expect(out).toEqual([{ down: 3, up: 4 }]);
  });

  it('addTransfers: POST /queues/{q}/transfers with urls and options, returns uuids', () => {
    let out: unknown;
    api.addTransfers(['http://a', 'http://b'], OPTIONS).subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/queues/q1/transfers');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ urls: ['http://a', 'http://b'], ...WIRE_OPTIONS });
    req.flush({ uuids: ['u1', 'u2'] }, { status: 201, statusText: 'Created' });
    expect(out).toEqual(['u1', 'u2']);
  });

  it('addTransferWithData: POST multipart with file and options parts, returns first uuid', () => {
    let out: unknown;
    const file = new File(['abc'], 'x.torrent');
    api.addTransferWithData(file, OPTIONS).subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/queues/q1/transfers');
    expect(req.request.method).toBe('POST');
    const form = req.request.body as FormData;
    expect(form).toBeInstanceOf(FormData);
    expect((form.get('file') as File).name).toBe('x.torrent');
    expect(JSON.parse(form.get('options') as string)).toEqual(WIRE_OPTIONS);
    req.flush({ uuids: ['u1'] }, { status: 201, statusText: 'Created' });
    expect(out).toBe('u1');
  });

  it('getSettingsValues: GET /settings?key=.. and returns values in key order, null for unset', () => {
    let out: unknown;
    api.getSettingsValues(['b', 'a', 'c']).subscribe((v) => (out = v));
    const req = httpMock.expectOne((r) => r.url === '/api/v1/settings');
    expect(req.request.method).toBe('GET');
    expect(req.request.params.getAll('key')).toEqual(['b', 'a', 'c']);
    req.flush({ a: 1, b: 'two', c: null });
    expect(out).toEqual(['two', 1, null]);
  });

  it('setSettingsValues: PATCH /settings with a key->value object', () => {
    let done = false;
    api.setSettingsValues(['a', 'b'], [1, 'x']).subscribe(() => (done = true));
    const req = httpMock.expectOne('/api/v1/settings');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ a: 1, b: 'x' });
    req.flush(null, NO_CONTENT);
    expect(done).toBe(true);
  });

  it('applySettings: POST /settings/apply', () => {
    let done = false;
    api.applySettings().subscribe(() => (done = true));
    const req = httpMock.expectOne('/api/v1/settings/apply');
    expect(req.request.method).toBe('POST');
    req.flush(null, { status: 202, statusText: 'Accepted' });
    expect(done).toBe(true);
  });

  it('getSettingsPages: GET /settings/pages', () => {
    let out: unknown;
    api.getSettingsPages().subscribe((v) => (out = v));
    const req = httpMock.expectOne('/api/v1/settings/pages');
    expect(req.request.method).toBe('GET');
    const body = [{ title: 'T', script: 's', icon: 'i' }];
    req.flush(body);
    expect(out).toEqual(body);
  });
});
