import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { ApiError, RestClient } from './rest-client';

describe('RestClient', () => {
  let client: RestClient;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    client = TestBed.inject(RestClient);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('prefixes /api/v1 and uses the matching verb for every method', () => {
    client.get('/a').subscribe();
    expect(httpMock.expectOne('/api/v1/a').request.method).toBe('GET');
    client.post('/a', { x: 1 }).subscribe();
    const post = httpMock.expectOne('/api/v1/a');
    expect(post.request.method).toBe('POST');
    expect(post.request.body).toEqual({ x: 1 });
    client.put('/a', [1]).subscribe();
    expect(httpMock.expectOne('/api/v1/a').request.method).toBe('PUT');
    client.patch('/a', {}).subscribe();
    expect(httpMock.expectOne('/api/v1/a').request.method).toBe('PATCH');
    client.delete('/a').subscribe();
    expect(httpMock.expectOne('/api/v1/a').request.method).toBe('DELETE');
  });

  it('repeats array query parameters', () => {
    client.get('/settings', { key: ['a', 'b'] }).subscribe();
    const req = httpMock.expectOne((r) => r.url === '/api/v1/settings');
    expect(req.request.params.getAll('key')).toEqual(['a', 'b']);
  });

  it('maps the {error:{code,message}} envelope to an ApiError', () => {
    let err: unknown;
    client.get('/queues/x/transfers').subscribe({ error: (e) => (err = e) });
    httpMock
      .expectOne('/api/v1/queues/x/transfers')
      .flush(
        { error: { code: 'queue_not_found', message: 'No such queue' } },
        { status: 404, statusText: 'Not Found' },
      );
    expect(err).toBeInstanceOf(ApiError);
    const apiErr = err as ApiError;
    expect(apiErr.status).toBe(404);
    expect(apiErr.code).toBe('queue_not_found');
    expect(apiErr.message).toBe('No such queue');
  });

  it('falls back to statusText when the body is not an envelope', () => {
    let err: unknown;
    client.get('/queues').subscribe({ error: (e) => (err = e) });
    httpMock.expectOne('/api/v1/queues').flush('<html>', { status: 502, statusText: 'Bad Gateway' });
    const apiErr = err as ApiError;
    expect(apiErr).toBeInstanceOf(ApiError);
    expect(apiErr.status).toBe(502);
    expect(apiErr.code).toBe('unknown');
    expect(apiErr.message).toBe('Bad Gateway');
  });
});
