import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { App } from './app';
import { TransferApi } from './core/api/transfer-api';
import { XmlRpcTransferApi } from './core/api/xmlrpc-transfer-api';

describe('App', () => {
  let httpMock: HttpTestingController;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [App],
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        { provide: TransferApi, useClass: XmlRpcTransferApi },
      ],
    }).compileComponents();
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('should create the app', () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    expect(app).toBeTruthy();

    const req = httpMock.expectOne('/xmlrpc');
    req.flush(
      '<?xml version="1.0"?><methodResponse><params><param><value><array><data></data></array></value></param></params></methodResponse>',
    );
  });

  it('should render the scaffold heading', async () => {
    const fixture = TestBed.createComponent(App);
    await fixture.whenStable();

    const req = httpMock.expectOne('/xmlrpc');
    req.flush(
      '<?xml version="1.0"?><methodResponse><params><param><value><array><data></data></array></value></param></params></methodResponse>',
    );

    const compiled = fixture.nativeElement as HTMLElement;
    expect(compiled.querySelector('h1')?.textContent).toContain(
      'FatRat Web UI — Phase 1 scaffold',
    );
  });

  it('renders "Loaded 0 queue(s)" once getQueues() resolves with an empty list', async () => {
    const fixture = TestBed.createComponent(App);
    await fixture.whenStable();

    const req = httpMock.expectOne('/xmlrpc');
    expect(req.request.body).toContain('<methodName>getQueues</methodName>');
    req.flush(
      '<?xml version="1.0"?><methodResponse><params><param><value><array><data></data></array></value></param></params></methodResponse>',
    );
    fixture.detectChanges();

    const compiled = fixture.nativeElement as HTMLElement;
    expect(compiled.textContent).toContain('Loaded 0 queue(s) from the backend.');
  });
});
