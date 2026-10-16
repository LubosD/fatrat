import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { RestClient } from './api/rest-client';
import { AvailablePlugin, Plugin, PluginRepository } from './api/models';

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

  /** Installs, or updates an installed plugin to what the repository now offers. */
  install(repository: string, plugin: string): Observable<{ restartRequired: boolean }> {
    return this.rest.post<{ restartRequired: boolean }>('/plugins', { repository, plugin });
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
