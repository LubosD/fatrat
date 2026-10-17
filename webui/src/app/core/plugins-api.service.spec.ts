import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { PluginsApi } from './plugins-api.service';

describe('PluginsApi', () => {
  let api: PluginsApi;
  let http: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    api = TestBed.inject(PluginsApi);
    http = TestBed.inject(HttpTestingController);
  });

  afterEach(() => http.verify());

  it('lists installed plugins', () => {
    let result: unknown;
    api.list().subscribe((v) => (result = v));
    const req = http.expectOne('/api/v1/plugins');
    expect(req.request.method).toBe('GET');
    req.flush([{ name: 'dummy', valid: true, version: '1.0.0' }]);
    expect(result).toEqual([{ name: 'dummy', valid: true, version: '1.0.0' }]);
  });

  it('asks for updates only when told to', () => {
    api.available().subscribe();
    http.expectOne('/api/v1/plugins/available').flush([]);

    api.available(true).subscribe();
    const req = http.expectOne((r) => r.url === '/api/v1/plugins/available');
    expect(req.request.params.get('updates')).toBe('true');
    req.flush([]);
  });

  it('installs, enables and removes a plugin', () => {
    api.install('example-plugins', 'dummy').subscribe();
    let req = http.expectOne('/api/v1/plugins');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ repository: 'example-plugins', plugin: 'dummy' });
    req.flush({ plugin: 'x', state: 'loaded', activeTransfers: 0, pendingReload: false });

    api.setEnabled('a plugin', false).subscribe();
    req = http.expectOne('/api/v1/plugins/a%20plugin');
    expect(req.request.method).toBe('PATCH');
    expect(req.request.body).toEqual({ enabled: false });
    req.flush({ name: 'a plugin', valid: true, enabled: false });

    api.uninstall('a plugin').subscribe();
    req = http.expectOne('/api/v1/plugins/a%20plugin');
    expect(req.request.method).toBe('DELETE');
    req.flush(null, { status: 204, statusText: 'No Content' });
  });

  it('manages repositories', () => {
    api.repositories().subscribe();
    http.expectOne('/api/v1/plugin-repositories').flush([]);

    api.addRepository('https://example.com/p.git').subscribe();
    let req = http.expectOne('/api/v1/plugin-repositories');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ source: 'https://example.com/p.git' });
    req.flush({ id: 'p', source: 'https://example.com/p.git', local: false, name: 'p', description: '', revision: 'abc' });

    api.refresh().subscribe();
    req = http.expectOne('/api/v1/plugin-repositories/refresh');
    expect(req.request.method).toBe('POST');
    req.flush([]);

    api.removeRepository('p/1').subscribe();
    req = http.expectOne('/api/v1/plugin-repositories/p%2F1');
    expect(req.request.method).toBe('DELETE');
    req.flush(null, { status: 204, statusText: 'No Content' });
  });
});
