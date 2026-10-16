import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { RestClient } from './api/rest-client';
import { ScheduledAction, ScheduledActionProperties } from './api/models';

const e = encodeURIComponent;

@Injectable({ providedIn: 'root' })
export class SchedulerApi {
  private readonly rest = inject(RestClient);

  list(): Observable<ScheduledAction[]> {
    return this.rest.get<ScheduledAction[]>('/scheduler/actions');
  }

  /** name, queue and action are required, and so are time (repeated) or at (one time). */
  create(action: ScheduledActionProperties): Observable<ScheduledAction> {
    return this.rest.post<ScheduledAction>('/scheduler/actions', action);
  }

  update(id: string, changes: ScheduledActionProperties): Observable<ScheduledAction> {
    return this.rest.patch<ScheduledAction>(`/scheduler/actions/${e(id)}`, changes);
  }

  delete(id: string): Observable<void> {
    return this.rest.delete<void>(`/scheduler/actions/${e(id)}`);
  }
}
