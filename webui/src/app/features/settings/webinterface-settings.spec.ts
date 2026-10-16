import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { WebinterfaceSettings } from './webinterface-settings';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { WebinterfaceSettingsApiService } from '../../core/webinterface-settings-api.service';

const KEYS = ['remote/enable', 'remote/port', 'remote/password', 'remote/ssl', 'remote/ssl_pem'];

function configure(
  getSettingsValues = vi.fn().mockReturnValue(of([true, 8080, 'secret', false, ''])),
  draftGet = vi.fn().mockReturnValue(undefined),
  generateCertificate = vi.fn().mockReturnValue(of('-----BEGIN CERTIFICATE-----')),
) {
  const stage = vi.fn();
  TestBed.configureTestingModule({
    imports: [WebinterfaceSettings],
    providers: [
      { provide: TransferApi, useValue: { getSettingsValues } },
      { provide: SettingsDraftService, useValue: { get: draftGet, stage } },
      { provide: WebinterfaceSettingsApiService, useValue: { generateCertificate } },
    ],
  });
  return { getSettingsValues, stage, draftGet, generateCertificate };
}

describe('WebinterfaceSettings', () => {
  it('fetches all the expected keys on construction and populates fields', () => {
    const { getSettingsValues } = configure();
    const fixture = TestBed.createComponent(WebinterfaceSettings);
    fixture.detectChanges();

    expect(getSettingsValues).toHaveBeenCalledWith(KEYS);
    expect(fixture.componentInstance['enable']).toBe(true);
    expect(fixture.componentInstance['port']).toBe(8080);
    expect(fixture.componentInstance['password']).toBe('secret');
  });

  it('prefers a staged draft value over the fetched value', () => {
    const draftGet = vi.fn().mockImplementation((key: string) => (key === 'remote/port' ? 9999 : undefined));
    configure(undefined, draftGet);
    const fixture = TestBed.createComponent(WebinterfaceSettings);
    fixture.detectChanges();

    expect(fixture.componentInstance['port']).toBe(9999);
  });

  it('stages a field change under the correct key', () => {
    const { stage } = configure();
    const fixture = TestBed.createComponent(WebinterfaceSettings);
    fixture.detectChanges();

    fixture.componentInstance['stage']('remote/port', 443);

    expect(stage).toHaveBeenCalledWith('remote/port', 443);
  });

  it('opens the certificate dialog with the current hostname prefilled', () => {
    configure();
    const fixture = TestBed.createComponent(WebinterfaceSettings);
    fixture.detectChanges();

    fixture.componentInstance['openCertDialog']();

    expect(fixture.componentInstance['showCertDialog']()).toBe(true);
    expect(fixture.componentInstance['certHostname']).toBe(window.location.hostname);
  });

  it('generates a certificate, stages it, and closes the dialog', () => {
    const { generateCertificate, stage } = configure();
    const fixture = TestBed.createComponent(WebinterfaceSettings);
    fixture.detectChanges();
    fixture.componentInstance['certHostname'] = 'example.com';

    fixture.componentInstance['generateCertificate']();

    expect(generateCertificate).toHaveBeenCalledWith('example.com');
    expect(stage).toHaveBeenCalledWith('remote/ssl_pem', '-----BEGIN CERTIFICATE-----');
    expect(fixture.componentInstance['sslPem']).toBe('-----BEGIN CERTIFICATE-----');
    expect(fixture.componentInstance['showCertDialog']()).toBe(false);
  });
});
