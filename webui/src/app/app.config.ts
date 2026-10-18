import { ApplicationConfig, provideBrowserGlobalErrorListeners } from '@angular/core';
import { provideHttpClient } from '@angular/common/http';
import { providePrimeNG } from 'primeng/config';
import { ConfirmationService, MessageService } from 'primeng/api';
import { TransferApi } from './core/api/transfer-api';
import { RestTransferApi } from './core/api/rest-transfer-api';
import { FatRatPreset } from './theme';

export const appConfig: ApplicationConfig = {
  providers: [
    provideBrowserGlobalErrorListeners(),
    provideHttpClient(),
    // The logo and the legacy look are light-only; the selector is never applied, so dark mode
    // stays off regardless of the OS colour-scheme preference (Aura's default is 'system').
    providePrimeNG({ theme: { preset: FatRatPreset, options: { darkModeSelector: '.app-dark' } } }),
    { provide: TransferApi, useClass: RestTransferApi },
    ConfirmationService,
    MessageService,
  ]
};
