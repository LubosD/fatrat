import { TestBed } from '@angular/core/testing';
import { Subject, of, throwError } from 'rxjs';
import { MenuItem, MessageService } from 'primeng/api';
import { PluginActions } from './plugin-actions';
import { TasksApi } from '../../core/tasks-api.service';
import { ApiError } from '../../core/api/rest-client';
import { PluginTransferAction } from '../../core/api/models';

function setup(actions: () => ReturnType<TasksApi['actions']>) {
  const api = { actions: vi.fn(actions), run: vi.fn().mockReturnValue(of('k1')) };
  const messages = { add: vi.fn() };
  TestBed.configureTestingModule({
    providers: [
      { provide: TasksApi, useValue: api },
      { provide: MessageService, useValue: messages },
    ],
  });
  return { service: TestBed.inject(PluginActions), api, messages };
}

describe('PluginActions', () => {
  it('shows a placeholder until the actions arrive', () => {
    const reply = new Subject<PluginTransferAction[]>();
    const { service } = setup(() => reply);
    const seen: MenuItem[][] = [];
    service.menuItems('t1').subscribe((items) => seen.push(items));

    expect(seen[0].map((i) => i.label ?? '—')).toEqual(['—', 'Loading…']);
    reply.next([{ id: 'u.unpack', label: 'Unpack', icon: 'archive' }]);
    expect(seen[1].map((i) => i.label ?? '—')).toEqual(['—', 'Unpack']);
  });

  it('adds nothing when there are no actions or the call fails', () => {
    const { service } = setup(() => throwError(() => new ApiError(500, 'x', 'x')));
    const seen: MenuItem[][] = [];
    service.menuItems('t1').subscribe((items) => seen.push(items));
    expect(seen.at(-1)).toEqual([]);
  });

  it('runs an action and says when it started a task', () => {
    const { service, api, messages } = setup(() => of([{ id: 'u.unpack', label: 'Unpack', icon: '' }]));
    let items: MenuItem[] = [];
    service.menuItems('t1').subscribe((i) => (items = i));

    items[1].command!({});
    expect(api.run).toHaveBeenCalledWith('t1', 'u.unpack');
    expect(messages.add).toHaveBeenCalledWith(expect.objectContaining({ summary: 'Unpack started' }));
  });

  it('reports a failed action', () => {
    const { service, api, messages } = setup(() => of([{ id: 'u.unpack', label: 'Unpack', icon: '' }]));
    api.run.mockReturnValue(throwError(() => new ApiError(409, 'not_applicable', 'The action does not apply')));
    let items: MenuItem[] = [];
    service.menuItems('t1').subscribe((i) => (items = i));

    items[1].command!({});
    expect(messages.add).toHaveBeenCalledWith(expect.objectContaining({ severity: 'error', detail: 'The action does not apply' }));
  });
});
