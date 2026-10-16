import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { RssApi } from './rss-api.service';
import { ApiError } from './api/rest-client';

describe('RssApi', () => {
  let api: RssApi;
  let http: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    api = TestBed.inject(RssApi);
    http = TestBed.inject(HttpTestingController);
  });

  afterEach(() => http.verify());

  it('lists feeds', () => {
    let result: unknown;
    api.listFeeds().subscribe((v) => (result = v));
    const req = http.expectOne('/api/v1/rss/feeds');
    expect(req.request.method).toBe('GET');
    req.flush([{ id: 'f1', name: 'A', url: 'http://a' }]);
    expect(result).toEqual([{ id: 'f1', name: 'A', url: 'http://a' }]);
  });

  it('creates, updates and deletes a feed by id', () => {
    api.createFeed({ name: 'A', url: 'http://a' }).subscribe();
    let req = http.expectOne('/api/v1/rss/feeds');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ name: 'A', url: 'http://a' });
    req.flush({ id: 'f1', name: 'A', url: 'http://a' });

    api.updateFeed('f 1', { name: 'B' }).subscribe();
    req = http.expectOne('/api/v1/rss/feeds/f%201');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ name: 'B' });
    req.flush({ id: 'f 1', name: 'B', url: 'http://a' });

    api.deleteFeed('f1').subscribe();
    req = http.expectOne('/api/v1/rss/feeds/f1');
    expect(req.request.method).toBe('DELETE');
    req.flush(null, { status: 204, statusText: 'No Content' });
  });

  it('creates, updates and deletes a regexp by id', () => {
    api.createRegexp({ feedId: 'f1', queue: 'q1', expression: 'x' }).subscribe();
    let req = http.expectOne('/api/v1/rss/regexps');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ feedId: 'f1', queue: 'q1', expression: 'x' });
    req.flush({});

    api.updateRegexp('r1', { addPaused: true }).subscribe();
    req = http.expectOne('/api/v1/rss/regexps/r1');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ addPaused: true });
    req.flush({});

    api.deleteRegexp('r1').subscribe();
    req = http.expectOne('/api/v1/rss/regexps/r1');
    expect(req.request.method).toBe('DELETE');
    req.flush(null, { status: 204, statusText: 'No Content' });
  });

  it('gets and replaces the downloaded episodes as a plain array', () => {
    let result: string[] = [];
    api.getDownloaded('r1').subscribe((v) => (result = v));
    let req = http.expectOne('/api/v1/rss/regexps/r1/downloaded');
    expect(req.request.method).toBe('GET');
    req.flush(['S01E01']);
    expect(result).toEqual(['S01E01']);

    api.setDownloaded('r1', ['S01E01', 'S01E02']).subscribe();
    req = http.expectOne('/api/v1/rss/regexps/r1/downloaded');
    expect(req.request.method).toBe('PUT');
    expect(req.request.body).toEqual(['S01E01', 'S01E02']);
    req.flush(null, { status: 204, statusText: 'No Content' });
  });

  it('posts a regexp test and a refresh', () => {
    api.test({ expression: 'a', text: 'b', tvs: 'season' }).subscribe();
    let req = http.expectOne('/api/v1/rss/test');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ expression: 'a', text: 'b', tvs: 'season' });
    req.flush({ matches: false });

    api.refresh().subscribe();
    req = http.expectOne('/api/v1/rss/refresh');
    expect(req.request.method).toBe('POST');
    req.flush(null, { status: 202, statusText: 'Accepted' });
  });

  it('surfaces the server error envelope', () => {
    let error: unknown;
    api.createFeed({ name: '', url: '' }).subscribe({ error: (e) => (error = e) });
    http
      .expectOne('/api/v1/rss/feeds')
      .flush({ error: { code: 'invalid_property', message: 'Property "name" must not be empty' } }, { status: 422, statusText: 'Unprocessable' });
    expect(error).toBeInstanceOf(ApiError);
    expect((error as ApiError).code).toBe('invalid_property');
    expect((error as ApiError).message).toContain('must not be empty');
  });
});
