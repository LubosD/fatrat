import { TestBed } from '@angular/core/testing';
import { Subject, of } from 'rxjs';
import { TasksStore } from './tasks.store';
import { TasksApi } from '../core/tasks-api.service';
import { PushSocketService, TaskPushEvent } from '../core/push-socket.service';
import { PluginTask } from '../core/api/models';

export function makeTask(overrides: Partial<PluginTask> = {}): PluginTask {
  return {
    id: 'k1', plugin: 'p.so', title: 'Task', transfer: 't1', transferName: 'file.rar',
    state: 'running', progress: -1, status: '', message: null, ...overrides,
  };
}

function setup(initial: PluginTask[] = []) {
  const taskEvents$ = new Subject<TaskPushEvent>();
  const connected$ = new Subject<void>();
  const list = vi.fn().mockReturnValue(of(initial));
  TestBed.configureTestingModule({
    providers: [
      { provide: TasksApi, useValue: { list } },
      { provide: PushSocketService, useValue: { taskEvents$, connected$ } },
    ],
  });
  return { store: TestBed.inject(TasksStore), taskEvents$, connected$, list };
}

describe('TasksStore', () => {
  it('loads the tasks at start and again on every reconnect', () => {
    const { store, connected$, list } = setup([makeTask()]);
    expect(store.tasks().map((t) => t.id)).toEqual(['k1']);
    connected$.next();
    expect(list).toHaveBeenCalledTimes(2);
  });

  it('applies pushed changes in place, appends new tasks and drops removed ones', () => {
    const { store, taskEvents$ } = setup([makeTask({ id: 'a' }), makeTask({ id: 'b' })]);
    taskEvents$.next({ type: 'task', task: makeTask({ id: 'a', progress: 50 }) });
    taskEvents$.next({ type: 'task', task: makeTask({ id: 'c' }) });
    taskEvents$.next({ type: 'task-removed', id: 'b' });
    expect(store.tasks().map((t) => [t.id, t.progress])).toEqual([['a', 50], ['c', -1]]);
  });

  it('lists open questions, hiding those answered here until the server catches up', () => {
    const prompt = { id: 'p1', form: { fields: [] }, expiresAt: '2030-01-01T00:00:00Z' };
    const { store } = setup([makeTask({ id: 'a', state: 'waiting', prompt }), makeTask({ id: 'b' })]);
    expect(store.openPrompts().map((t) => t.id)).toEqual(['a']);
    store.markHandled('p1');
    expect(store.openPrompts()).toEqual([]);
  });

  it('reports a task that fails while we watch, once', () => {
    const { store, taskEvents$ } = setup([makeTask({ id: 'a' })]);
    const failed: string[] = [];
    store.failures$.subscribe((t) => failed.push(t.id));
    taskEvents$.next({ type: 'task', task: makeTask({ id: 'a', state: 'failed', message: 'x' }) });
    taskEvents$.next({ type: 'task', task: makeTask({ id: 'a', state: 'failed', message: 'x' }) });
    expect(failed).toEqual(['a']);
  });

  it('remembers which task the user asked to answer', () => {
    const { store } = setup();
    store.request('k9');
    expect(store.requested()).toBe('k9');
  });
});
