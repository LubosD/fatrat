import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { SchedulerApi } from './scheduler-api.service';
import { ApiError } from './api/rest-client';

describe('SchedulerApi', () => {
  let api: SchedulerApi;
  let http: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    api = TestBed.inject(SchedulerApi);
    http = TestBed.inject(HttpTestingController);
  });

  afterEach(() => http.verify());

  it('lists the actions', () => {
    let result: unknown;
    api.list().subscribe((v) => (result = v));
    const req = http.expectOne('/api/v1/scheduler/actions');
    expect(req.request.method).toBe('GET');
    req.flush([{ id: 'a1', name: 'Night' }]);
    expect(result).toEqual([{ id: 'a1', name: 'Night' }]);
  });

  it('creates, updates and deletes an action by id', () => {
    api.create({ name: 'N', queue: '{q}', action: 'stop', time: '01:00' }).subscribe();
    let req = http.expectOne('/api/v1/scheduler/actions');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ name: 'N', queue: '{q}', action: 'stop', time: '01:00' });
    req.flush({ id: 'a1' });

    api.update('a 1', { name: 'M' }).subscribe();
    req = http.expectOne('/api/v1/scheduler/actions/a%201');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ name: 'M' });
    req.flush({ id: 'a 1' });

    api.delete('a 1').subscribe();
    req = http.expectOne('/api/v1/scheduler/actions/a%201');
    expect(req.request.method).toBe('DELETE');
    req.flush(null, { status: 204, statusText: 'No Content' });
  });

  it('turns an error response into an ApiError with the server message', () => {
    let error: ApiError | undefined;
    api.create({}).subscribe({ error: (err) => (error = err) });
    http.expectOne('/api/v1/scheduler/actions').flush(
      { error: { code: 'invalid_property', message: 'Property "name" is required' } },
      { status: 422, statusText: 'Unprocessable' },
    );
    expect(error?.message).toBe('Property "name" is required');
  });
});
