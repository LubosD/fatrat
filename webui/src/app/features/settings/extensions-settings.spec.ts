import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { signal } from '@angular/core';
import { ConfirmationService } from 'primeng/api';
import { ExtensionsSettings } from './extensions-settings';
import { PluginsApi } from '../../core/plugins-api.service';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { ViewportService } from '../../core/viewport.service';
import { ApiError } from '../../core/api/rest-client';
import { AvailablePlugin, Plugin, PluginRepository } from '../../core/api/models';

const repository: PluginRepository = {
  id: 'example-plugins', source: '/srv/plugins', local: true,
  name: 'example-plugins', description: 'examples', revision: '',
};

const plugin: Plugin = {
  name: 'dummy', valid: true, version: '1.0.0', description: 'a worked example', enabled: true,
  config: [
    { type: 'group', title: 'Account', items: [{ type: 'text', key: 'dummy/user', title: 'Username' }] },
    { type: 'checkbox', key: 'dummy/skip_wait', title: 'Skip the countdown' },
  ],
};

const available: AvailablePlugin[] = [
  { name: 'dummy', version: '1.1.0', description: 'a worked example', repository: 'example-plugins', installedVersion: '1.0.0' },
  { name: 'extras', version: '1.0.0', description: 'the other kinds', repository: 'example-plugins', installedVersion: '' },
];

function configure(opts: { plugins?: Plugin[]; api?: Record<string, unknown>; values?: unknown[] } = {}) {
  const confirm = vi.fn((c: { accept: () => void; message?: string }) => c.accept());
  const api = {
    list: vi.fn().mockReturnValue(of(opts.plugins ?? [plugin])),
    available: vi.fn().mockReturnValue(of(available)),
    repositories: vi.fn().mockReturnValue(of([repository])),
    install: vi.fn().mockReturnValue(of({ restartRequired: true })),
    uninstall: vi.fn().mockReturnValue(of(undefined)),
    setEnabled: vi.fn().mockReturnValue(of(plugin)),
    addRepository: vi.fn().mockReturnValue(of(repository)),
    removeRepository: vi.fn().mockReturnValue(of(undefined)),
    refresh: vi.fn().mockReturnValue(of([repository])),
    ...opts.api,
  };
  const transferApi = {
    getSettingsValues: vi.fn().mockReturnValue(of(opts.values ?? ['someone', true])),
  };
  const draft = new SettingsDraftService();

  TestBed.configureTestingModule({
    imports: [ExtensionsSettings],
    providers: [
      { provide: PluginsApi, useValue: api },
      { provide: TransferApi, useValue: transferApi },
      { provide: SettingsDraftService, useValue: draft },
      { provide: ConfirmationService, useValue: { confirm } },
      { provide: ViewportService, useValue: { isMobile: signal(false) } },
    ],
  });

  const fixture = TestBed.createComponent(ExtensionsSettings);
  fixture.detectChanges();
  return { fixture, c: fixture.componentInstance as unknown as Record<string, any>, api, draft, confirm, transferApi };
}

const apiError = (message: string) => throwError(() => ({ message }) as ApiError);

describe('ExtensionsSettings', () => {
  it('lists repositories, what they offer and what is installed', () => {
    const { fixture } = configure();
    const text = fixture.nativeElement.textContent;

    expect(text).toContain('example-plugins');
    expect(text).toContain('/srv/plugins');
    expect(text).toContain('dummy');
    expect(text).toContain('extras');
  });

  it('offers to update a plugin whose repository has moved on', () => {
    const { c } = configure();

    expect(c['isUpdate'](available[0])).toBe(true);
    expect(c['isUpdate'](available[1])).toBe(false);
  });

  it('warns about trust before adding a repository, and only adds on accept', () => {
    const { c, api, confirm } = configure();

    c['newRepository'] = 'https://example.com/p.git';
    c['addRepository']();

    expect(confirm).toHaveBeenCalled();
    expect(confirm.mock.calls[0][0].message).toContain('full privileges');
    expect(api.addRepository).toHaveBeenCalledWith('https://example.com/p.git');
  });

  it('does not add anything when the confirmation is declined', () => {
    const confirm = vi.fn();
    const { c, api } = configure({ api: {} });

    // Replace the auto-accepting confirmation with one that never accepts.
    (c as any).confirmation = { confirm };
    c['newRepository'] = 'https://example.com/p.git';
    c['addRepository']();

    expect(api.addRepository).not.toHaveBeenCalled();
  });

  it('ignores an empty repository field', () => {
    const { c, api } = configure();

    c['newRepository'] = '   ';
    c['addRepository']();

    expect(api.addRepository).not.toHaveBeenCalled();
  });

  it('says a restart is needed after installing', () => {
    const { c, api } = configure();

    c['install'](available[1]);

    expect(api.install).toHaveBeenCalledWith('example-plugins', 'extras');
    expect(c['message']()).toContain('Restart FatRat');
  });

  it('does not offer a text box for a settings array it would overwrite', () => {
    const withOrder: Plugin = {
      ...plugin,
      config: [
        { type: 'text', key: 'dummy/user', title: 'Username' },
        { type: 'order', key: 'dummy/mirrors', title: 'Mirrors', options: [{ value: 'eu' }] },
      ],
    };
    const { fixture, c, transferApi } = configure({ plugins: [withOrder], values: ['someone'] });

    // The order item is shown, so the user knows the setting exists...
    expect(fixture.nativeElement.querySelector('[data-testid="ext-field-readonly"]').textContent)
      .toContain('desktop interface');
    // ...but its value is neither read nor bound to a field.
    expect(transferApi.getSettingsValues).toHaveBeenCalledWith(['dummy/user']);
    expect(c['fields']().find((f: any) => f.item.key === 'dummy/mirrors').editable).toBe(false);
    expect(c['fields']().find((f: any) => f.item.key === 'dummy/user').value).toBe('someone');
  });

  it('flattens a plugin config into fields and stages edits for the OK button', () => {
    const { c, draft } = configure();
    const fields = c['fields']();

    expect(fields.map((f: any) => f.item.key)).toEqual(['dummy/user', 'dummy/skip_wait']);
    expect(fields[0].group).toContain('Account');
    expect(fields[0].value).toBe('someone');

    c['stage']('dummy/user', 'nobody');

    expect(draft.get('dummy/user')).toBe('nobody');
  });

  it('shows the error from a failed call instead of failing silently', () => {
    const { c } = configure({ api: { list: vi.fn().mockReturnValue(apiError('nope')) } });

    expect(c['message']()).toBe('nope');
  });

  it('shows a broken plugin with its error rather than hiding it', () => {
    const { fixture } = configure({
      plugins: [{ name: 'broken', valid: false, error: 'plugin.json has no name' }],
    });

    expect(fixture.nativeElement.textContent).toContain('plugin.json has no name');
  });
});
