import { ChangeDetectorRef, Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ButtonDirective } from 'primeng/button';
import { Checkbox } from 'primeng/checkbox';
import { InputText } from 'primeng/inputtext';
import { Select } from 'primeng/select';
import { TableModule } from 'primeng/table';
import { ConfirmationService } from 'primeng/api';
import { ApiError } from '../../core/api/rest-client';
import { AvailablePlugin, Plugin, PluginConfigItem, PluginRepository } from '../../core/api/models';
import { JsonValue } from '../../core/api/json-value';
import { PluginsApi } from '../../core/plugins-api.service';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { TransferApi } from '../../core/api/transfer-api';
import { ViewportService } from '../../core/viewport.service';

/** A config item flattened for rendering, with its group's title carried along. */
interface Field {
  item: PluginConfigItem;
  group: string;
  value: JsonValue;
  /** False for item types this page cannot edit; it shows them instead of hiding them. */
  editable: boolean;
}

/** Item types this page has a field for. An `order` list is not one of them. */
const EDITABLE = new Set(['text', 'password', 'checkbox', 'combo']);

/**
 * Extensions: plugin repositories, what they offer, and the settings of what is installed.
 *
 * The settings fields are not hard-coded here — each plugin declares them in its plugin.json and
 * the backend passes that through, so this page renders whatever a plugin asks for, the same way
 * the desktop dialog does. Values are staged in the dialog's draft like every other settings page;
 * installs, removals and repository changes apply immediately (they are not settings).
 */
@Component({
  selector: 'app-extensions-settings',
  imports: [FormsModule, TableModule, ButtonDirective, Checkbox, InputText, Select],
  templateUrl: './extensions-settings.html',
})
export class ExtensionsSettings {
  private readonly api = inject(PluginsApi);
  private readonly transferApi = inject(TransferApi);
  private readonly draft = inject(SettingsDraftService);
  private readonly confirmation = inject(ConfirmationService);
  private readonly cdr = inject(ChangeDetectorRef);
  protected readonly viewport = inject(ViewportService);

  protected readonly plugins = signal<Plugin[]>([]);
  protected readonly repositories = signal<PluginRepository[]>([]);
  protected readonly available = signal<AvailablePlugin[]>([]);
  protected readonly fields = signal<Field[]>([]);

  protected readonly message = signal('');
  protected readonly busy = signal(false);
  protected newRepository = '';

  constructor() {
    this.reload();
  }

  protected reload(): void {
    this.api.repositories().subscribe({
      next: (repositories) => {
        this.repositories.set(repositories);
        this.cdr.markForCheck();
      },
      error: (err: ApiError) => this.fail(err),
    });
    this.api.available().subscribe({
      next: (available) => {
        this.available.set(available);
        this.cdr.markForCheck();
      },
      error: (err: ApiError) => this.fail(err),
    });
    this.api.list().subscribe({
      next: (plugins) => {
        this.plugins.set(plugins);
        this.loadFields(plugins);
      },
      error: (err: ApiError) => this.fail(err),
    });
  }

  /** What to show in place of a revision for a local repository. */
  protected shortRevision(repository: PluginRepository): string {
    return repository.local ? 'local directory' : repository.revision.slice(0, 10);
  }

  /** True when the repository offers a newer version than the one installed. */
  protected isUpdate(entry: AvailablePlugin): boolean {
    return !!entry.installedVersion && entry.installedVersion !== entry.version;
  }

  protected addRepository(): void {
    const source = this.newRepository.trim();
    if (!source || this.busy()) return;

    this.confirmation.confirm({
      header: 'Add plugin repository',
      message:
        `Add ${source}?\n\nPlugins you install from it run with the full privileges of the user ` +
        'running FatRat, like anything else installed from a git repository. Only add repositories you trust.',
      accept: () => this.run(this.api.addRepository(source), () => (this.newRepository = '')),
    });
  }

  protected removeRepository(repository: PluginRepository): void {
    this.confirmation.confirm({
      header: 'Forget repository',
      message: `Forget "${repository.name || repository.id}"? Plugins already installed from it stay installed.`,
      accept: () => this.run(this.api.removeRepository(repository.id)),
    });
  }

  protected refresh(): void {
    this.run(this.api.refresh());
  }

  protected install(entry: AvailablePlugin): void {
    const dependencies = entry.dependencies ?? [];

    if (!dependencies.length) {
      this.doInstall(entry, false);
      return;
    }

    // These come from PyPI, not from the repository the user chose to trust, so
    // they get their own question naming every package.
    this.confirmation.confirm({
      header: 'Install dependencies',
      message:
        `${entry.name} needs these packages from the Python Package Index:\n\n` +
        `${dependencies.join('\n')}\n\n` +
        'They will be downloaded with pip and installed for this plugin only. Install them?',
      accept: () => this.doInstall(entry, true),
    });
  }

  private doInstall(entry: AvailablePlugin, withDependencies: boolean): void {
    this.run(this.api.install(entry.repository, entry.name, withDependencies), () =>
      this.message.set(`${entry.name} installed. Restart FatRat to load it.`),
    );
  }

  protected uninstall(plugin: Plugin): void {
    this.confirmation.confirm({
      header: 'Remove plugin',
      message: `Remove "${plugin.name}"? Its files will be deleted.`,
      accept: () => this.run(this.api.uninstall(plugin.name)),
    });
  }

  protected setEnabled(plugin: Plugin, enabled: boolean): void {
    this.run(this.api.setEnabled(plugin.name, enabled), () =>
      this.message.set('Takes effect the next time FatRat starts.'),
    );
  }

  protected stage(key: string | undefined, value: JsonValue): void {
    if (key) this.draft.stage(key, value);
  }

  protected optionsOf(item: PluginConfigItem): { label: string; value: string }[] {
    return (item.options ?? []).map((o) => ({ label: o.title ?? o.value, value: o.value }));
  }

  /** Reads the current value of every key the installed plugins declare. */
  private loadFields(plugins: Plugin[]): void {
    const fields: Field[] = [];
    const add = (item: PluginConfigItem, group: string) =>
      fields.push({ item, group, value: null, editable: EDITABLE.has(item.type) && !!item.key });

    for (const plugin of plugins) {
      if (!plugin.valid) continue;
      for (const item of plugin.config ?? []) {
        if (item.type === 'group') {
          for (const child of item.items ?? []) add(child, `${plugin.name} — ${item.title ?? ''}`);
        } else {
          add(item, plugin.name);
        }
      }
    }

    // Only the editable ones: an `order` list is stored as a settings array, and
    // a text field bound to it would write a string over the array.
    const keys = fields.filter((f) => f.editable).map((f) => f.item.key as string);

    if (!keys.length) {
      this.fields.set(fields);
      this.cdr.markForCheck();
      return;
    }

    this.transferApi.getSettingsValues(keys).subscribe({
      next: (values) => {
        const byKey = new Map<string, JsonValue>(keys.map((key, i) => [key, values[i]]));

        for (const field of fields) {
          if (!field.editable) continue;
          const key = field.item.key as string;
          field.value = this.draft.get(key) ?? byKey.get(key) ?? (field.item.type === 'checkbox' ? false : '');
        }
        this.fields.set(fields);
        this.cdr.markForCheck();
      },
      error: (err: ApiError) => this.fail(err),
    });
  }

  private run<T>(call: { subscribe: (observer: { next: (v: T) => void; error: (e: ApiError) => void }) => void },
                 done?: () => void): void {
    this.busy.set(true);
    this.message.set('');
    call.subscribe({
      next: () => {
        this.busy.set(false);
        done?.();
        this.reload();
      },
      error: (err: ApiError) => {
        this.busy.set(false);
        this.fail(err);
      },
    });
  }

  private fail(err: ApiError): void {
    this.message.set(err.message);
    this.cdr.markForCheck();
  }
}
