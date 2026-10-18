import { Injectable, inject } from '@angular/core';
import { Observable, of } from 'rxjs';
import { catchError, map, startWith } from 'rxjs/operators';
import { MenuItem, MessageService } from 'primeng/api';
import { TasksApi } from '../../core/tasks-api.service';
import { ApiError } from '../../core/api/rest-client';
import { PluginTransferAction } from '../../core/api/models';

const LOADING: MenuItem[] = [{ separator: true }, { label: 'Loading…', disabled: true }];

/**
 * What plugins add to a transfer's menu, fetched each time a menu opens for a single transfer:
 * whether an action applies can depend on the transfer's current state. Plugin icons are desktop
 * theme names, so the web UI uses one generic icon.
 */
@Injectable({ providedIn: 'root' })
export class PluginActions {
  private readonly api = inject(TasksApi);
  private readonly messages = inject(MessageService);

  /** A separator and a placeholder at once, then the actions (nothing if there are none). */
  menuItems(transfer: string): Observable<MenuItem[]> {
    return this.api.actions(transfer).pipe(
      map((actions) => (actions.length ? [{ separator: true }, ...actions.map((a) => this.item(transfer, a))] : [])),
      catchError(() => of([])),
      startWith(LOADING),
    );
  }

  private item(transfer: string, action: PluginTransferAction): MenuItem {
    return { label: action.label, icon: 'pi pi-bolt', command: () => this.run(transfer, action) };
  }

  private run(transfer: string, action: PluginTransferAction): void {
    this.api.run(transfer, action.id).subscribe({
      next: (task) => {
        if (task) this.messages.add({ severity: 'info', summary: `${action.label} started` });
      },
      error: (err: ApiError) => this.messages.add({ severity: 'error', summary: action.label, detail: err.message }),
    });
  }
}
