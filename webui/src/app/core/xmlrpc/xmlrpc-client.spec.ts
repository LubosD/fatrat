import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { XmlRpcClient } from './xmlrpc-client';
import { XmlRpcFault } from './xmlrpc-value';

describe('XmlRpcClient', () => {
  let client: XmlRpcClient;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting(), XmlRpcClient],
    });
    client = TestBed.inject(XmlRpcClient);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  // Vitest 4's TestFunction no longer supports Jest-style `(done) => {}` callbacks
  // (its typings only pass a TestContext), so each test below returns a Promise that
  // resolves where the plan's `done()` call would have been - same async shape, adapted
  // minimally for this project's Vitest version.
  it('POSTs an encoded method call to /xmlrpc and decodes the response', () => {
    return new Promise<void>((done) => {
      client.call('getQueues').subscribe((result) => {
        expect(result).toEqual([]);
        done();
      });

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.method).toBe('POST');
      expect(req.request.body).toContain('<methodName>getQueues</methodName>');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><array><data></data></array></value></param></params></methodResponse>',
      );
    });
  });

  it('passes params through the encoder', () => {
    return new Promise<void>((done) => {
      client.call('Queue_getTransfers', 'uuid-1').subscribe(() => done());

      const req = httpMock.expectOne('/xmlrpc');
      expect(req.request.body).toContain('<value><string>uuid-1</string></value>');
      req.flush(
        '<?xml version="1.0"?><methodResponse><params><param><value><array><data></data></array></value></param></params></methodResponse>',
      );
    });
  });

  it('propagates XmlRpcFault from a fault response', () => {
    return new Promise<void>((done) => {
      client.call('Queue_getTransfers', 'bad-uuid').subscribe({
        error: (err) => {
          expect(err).toBeInstanceOf(XmlRpcFault);
          expect((err as XmlRpcFault).faultCode).toBe(101);
          done();
        },
      });

      const req = httpMock.expectOne('/xmlrpc');
      req.flush(
        '<?xml version="1.0"?><methodResponse><fault><value><struct>' +
          '<member><name>faultCode</name><value><int>101</int></value></member>' +
          '<member><name>faultString</name><value><string>Invalid queue UUID</string></value></member>' +
          '</struct></value></fault></methodResponse>',
      );
    });
  });
});
