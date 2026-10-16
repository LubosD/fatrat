import { Injectable } from '@angular/core';
import { JsonValue } from './api/json-value';

/**
 * Dialog-scoped (provided on SettingsDialog, not providedIn: 'root') staging area for settings
 * edits. Collects key -> value pairs as the user edits fields across however many settings pages
 * they visit in one dialog session, so a single OK click can batch-commit everything in one
 * Settings.setValue call, matching the legacy dialog's actual behavior. The dialog must call
 * clear() every time it's opened (not just rely on construction) since it keeps this service
 * instance alive across visible=false/true toggles, same as every other dialog in this app.
 */
@Injectable()
export class SettingsDraftService {
  private readonly staged = new Map<string, JsonValue>();

  stage(key: string, value: JsonValue): void {
    this.staged.set(key, value);
  }

  get(key: string): JsonValue | undefined {
    return this.staged.get(key);
  }

  getKeys(): string[] {
    return [...this.staged.keys()];
  }

  getValues(): JsonValue[] {
    return [...this.staged.values()];
  }

  clear(): void {
    this.staged.clear();
  }
}
