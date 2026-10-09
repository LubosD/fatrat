import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { WebinterfaceSettingsApiService } from './webinterface-settings-api.service';
import { XmlRpcClient } from './xmlrpc/xmlrpc-client';

describe('WebinterfaceSettingsApiService', () => {
  it('calls HttpService.generateCertificate with the hostname and returns the PEM text', () => {
    return new Promise<void>((done) => {
      const call = vi.fn().mockReturnValue(of('-----BEGIN CERTIFICATE-----...'));
      TestBed.configureTestingModule({
        providers: [WebinterfaceSettingsApiService, { provide: XmlRpcClient, useValue: { call } }],
      });
      const service = TestBed.inject(WebinterfaceSettingsApiService);

      service.generateCertificate('example.com').subscribe((pem) => {
        expect(call).toHaveBeenCalledWith('HttpService.generateCertificate', 'example.com');
        expect(pem).toBe('-----BEGIN CERTIFICATE-----...');
        done();
      });
    });
  });
});
