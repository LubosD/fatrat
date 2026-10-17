import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { RestClient } from './api/rest-client';
import { AvailablePlugin, Plugin, PluginInstallResult, PluginRepository } from './api/models';

const e = encodeURIComponent;

/**
 * Extensions: the repositories the user has added, and the plugins installed from them.
 *
 * Installing and refreshing talk to git on the server and can take seconds, so the UI should
 * show that something is happening rather than assume these return promptly.
 */
@Injectable({ providedIn: 'root' })
export class PluginsApi {
  private readonly rest = inject(RestClient);

  list(): Observable<Plugin[]> {
    return this.rest.get<Plugin[]>('/plugins');
  }

  available(updatesOnly = false): Observable<AvailablePlugin[]> {
    return this.rest.get<AvailablePlugin[]>('/plugins/available', updatesOnly ? { updates: true } : undefined);
  }

  /**
   * Installs, or updates an installed plugin to what the repository now offers.
   *
   * A plugin that declares dependencies is refused unless `withDependencies` is set: those
   * packages come from PyPI rather than from the repository the user chose, so the server
   * insists on being told yes to them specifically.
   */
  install(repository: string, plugin: string, withDependencies = false): Observable<PluginInstallResult> {
    const body: Record<string, unknown> = { repository, plugin };
    if (withDependencies) body['withDependencies'] = true;
    return this.rest.post<PluginInstallResult>('/plugins', body);
  }

  setEnabled(plugin: string, enabled: boolean): Observable<Plugin> {
    return this.rest.patch<Plugin>(`/plugins/${e(plugin)}`, { enabled });
  }

  uninstall(plugin: string): Observable<void> {
    return this.rest.delete<void>(`/plugins/${e(plugin)}`);
  }

  repositories(): Observable<PluginRepository[]> {
    return this.rest.get<PluginRepository[]>('/plugin-repositories');
  }

  /** `source` is a git URL, or a directory path on the server. */
  addRepository(source: string): Observable<PluginRepository> {
    return this.rest.post<PluginRepository>('/plugin-repositories', { source });
  }

  removeRepository(id: string): Observable<void> {
    return this.rest.delete<void>(`/plugin-repositories/${e(id)}`);
  }

  refresh(): Observable<PluginRepository[]> {
    return this.rest.post<PluginRepository[]>('/plugin-repositories/refresh', {});
  }
}
