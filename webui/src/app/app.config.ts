import { ApplicationConfig, provideBrowserGlobalErrorListeners } from '@angular/core';
import { provideHttpClient } from '@angular/common/http';
import { providePrimeNG } from 'primeng/config';
import Aura from '@primeng/themes/aura';
import { TransferApi } from './core/api/transfer-api';
import { XmlRpcTransferApi } from './core/api/xmlrpc-transfer-api';

export const appConfig: ApplicationConfig = {
  providers: [
    provideBrowserGlobalErrorListeners(),
    provideHttpClient(),
    providePrimeNG({ theme: { preset: Aura } }),
    { provide: TransferApi, useClass: XmlRpcTransferApi },
  ]
};
