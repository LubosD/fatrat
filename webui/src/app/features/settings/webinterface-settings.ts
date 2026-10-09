import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { InputText } from 'primeng/inputtext';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { Dialog } from 'primeng/dialog';
import { ButtonDirective } from 'primeng/button';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { WebinterfaceSettingsApiService } from '../../core/webinterface-settings-api.service';
import { XmlRpcValue } from '../../core/xmlrpc/xmlrpc-value';

const KEYS = ['remote/enable', 'remote/port', 'remote/password', 'remote/ssl', 'remote/ssl_pem'] as const;

@Component({
  selector: 'app-webinterface-settings',
  imports: [FormsModule, InputText, InputNumber, Checkbox, Dialog, ButtonDirective],
  templateUrl: './webinterface-settings.html',
})
export class WebinterfaceSettings {
  private readonly transferApi = inject(TransferApi);
  private readonly draft = inject(SettingsDraftService);
  private readonly certApi = inject(WebinterfaceSettingsApiService);

  protected enable = false;
  protected port = 0;
  protected password = '';
  protected ssl = false;
  protected sslPem = '';

  protected readonly showCertDialog = signal(false);
  protected certHostname = '';

  constructor() {
    this.transferApi.getSettingsValues([...KEYS]).subscribe({
      next: (values) => this.applyLoadedValues(values),
      error: (err) => console.error('Failed to load web interface settings', err),
    });
  }

  protected stage(key: string, value: XmlRpcValue): void {
    this.draft.stage(key, value);
  }

  protected openCertDialog(): void {
    this.certHostname = window.location.hostname;
    this.showCertDialog.set(true);
  }

  protected generateCertificate(): void {
    this.certApi.generateCertificate(this.certHostname).subscribe({
      next: (pem) => {
        this.sslPem = pem;
        this.stage('remote/ssl_pem', pem);
        this.showCertDialog.set(false);
      },
      error: (err) => console.error('Failed to generate certificate', err),
    });
  }

  private applyLoadedValues(values: XmlRpcValue[]): void {
    const raw = new Map<string, XmlRpcValue>(KEYS.map((key, i) => [key, values[i]]));
    const get = (key: string): XmlRpcValue | undefined => this.draft.get(key) ?? raw.get(key);

    this.enable = Boolean(get('remote/enable') ?? false);
    this.port = Number(get('remote/port') ?? 0);
    this.password = String(get('remote/password') ?? '');
    this.ssl = Boolean(get('remote/ssl') ?? false);
    this.sslPem = String(get('remote/ssl_pem') ?? '');
  }
}
