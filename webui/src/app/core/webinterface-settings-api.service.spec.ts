import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { WebinterfaceSettingsApiService } from './webinterface-settings-api.service';

describe('WebinterfaceSettingsApiService', () => {
  it('POSTs the hostname to the certificate endpoint and returns the path', () => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    const service = TestBed.inject(WebinterfaceSettingsApiService);
    const httpMock = TestBed.inject(HttpTestingController);
    let result = '';

    service.generateCertificate('example.com').subscribe((v) => (result = v));

    const req = httpMock.expectOne('/api/v1/settings/web-interface/certificate');
    expect(req.request.method).toBe('POST');
    expect(req.request.body).toEqual({ hostname: 'example.com' });
    req.flush({ path: '/home/u/.config/fatrat/cert.pem' });
    expect(result).toBe('/home/u/.config/fatrat/cert.pem');
    httpMock.verify();
  });
});
