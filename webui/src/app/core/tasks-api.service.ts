import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { RestClient } from './api/rest-client';
import { JsonValue } from './api/json-value';
import { PluginTask, PluginTransferAction } from './api/models';

const e = encodeURIComponent;

/**
 * What plugins add: actions on a transfer, and the tasks those start. A task can stop to ask a
 * question; answers go back over REST (the WebSocket only ever pushes).
 */
@Injectable({ providedIn: 'root' })
export class TasksApi {
  private readonly rest = inject(RestClient);

  actions(transfer: string): Observable<PluginTransferAction[]> {
    return this.rest.get<PluginTransferAction[]>(`/transfers/${e(transfer)}/actions`);
  }

  /** Resolves to the id of the task the action started, or null if it was done at once. */
  run(transfer: string, action: string): Observable<string | null> {
    return this.rest
      .post<{ task?: string } | null>(`/transfers/${e(transfer)}/actions/${e(action)}`)
      .pipe(map((body) => body?.task ?? null));
  }

  list(): Observable<PluginTask[]> {
    return this.rest.get<PluginTask[]>('/tasks');
  }

  answer(task: string, prompt: string, values: Record<string, JsonValue>): Observable<PluginTask> {
    return this.rest.post<PluginTask>(`/tasks/${e(task)}/prompts/${e(prompt)}`, { values });
  }

  /** Closes the question without an answer; the plugin is told nobody answered. */
  dismissPrompt(task: string, prompt: string): Observable<void> {
    return this.rest.delete<void>(`/tasks/${e(task)}/prompts/${e(prompt)}`);
  }

  /** Asks a running task to stop, or removes a finished one from the list. */
  cancelOrDismiss(task: string): Observable<unknown> {
    return this.rest.delete<unknown>(`/tasks/${e(task)}`);
  }
}
