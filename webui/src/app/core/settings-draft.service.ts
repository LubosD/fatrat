import { Injectable } from '@angular/core';
import { XmlRpcValue } from './xmlrpc/xmlrpc-value';

/**
 * Dialog-scoped (provided on SettingsDialog, not providedIn: 'root') staging area for settings
 * edits. Collects key -> value pairs as the user edits fields across however many settings pages
 * they visit in one dialog session, so a single OK click can batch-commit everything in one
 * Settings_setValue call, matching the legacy dialog's actual behavior. The dialog must call
 * clear() every time it's opened (not just rely on construction) since it keeps this service
 * instance alive across visible=false/true toggles, same as every other dialog in this app.
 */
@Injectable()
export class SettingsDraftService {
  private readonly staged = new Map<string, XmlRpcValue>();

  stage(key: string, value: XmlRpcValue): void {
    this.staged.set(key, value);
  }

  get(key: string): XmlRpcValue | undefined {
    return this.staged.get(key);
  }

  getKeys(): string[] {
    return [...this.staged.keys()];
  }

  getValues(): XmlRpcValue[] {
    return [...this.staged.values()];
  }

  clear(): void {
    this.staged.clear();
  }
}
