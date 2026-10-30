import { TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { Subject, of } from 'rxjs';
import { MessageService } from '@openng/optimus-ui/api';
import { TasksIndicator } from './tasks-indicator';
import { TasksStore } from '../../state/tasks.store';
import { TasksApi } from '../../core/tasks-api.service';
import { PluginTask } from '../../core/api/models';

function task(overrides: Partial<PluginTask>): PluginTask {
  return {
    id: 'k1', plugin: 'p.so', title: 'Unpack', transfer: 't1', transferName: 'file.rar',
    state: 'running', progress: 40, status: 'Extracting', message: null, ...overrides,
  };
}

function setup(tasks: PluginTask[]) {
  const failures$ = new Subject<PluginTask>();
  const added$ = new Subject<PluginTask>();
  const store = {
    tasks: signal(tasks),
    openPrompts: signal(tasks.filter((t) => t.prompt)),
    request: vi.fn(),
    remove: vi.fn(),
    failures$,
    added$,
  };
  const api = { cancelOrDismiss: vi.fn().mockReturnValue(of(null)) };
  const messages = { add: vi.fn() };
  TestBed.configureTestingModule({
    imports: [TasksIndicator],
    providers: [
      { provide: TasksStore, useValue: store },
      { provide: TasksApi, useValue: api },
      { provide: MessageService, useValue: messages },
    ],
  });
  const fixture = TestBed.createComponent(TasksIndicator);
  fixture.detectChanges();
  const c = fixture.componentInstance as unknown as {
    statusText(t: PluginTask): string;
    answer(t: PluginTask): void;
    stop(t: PluginTask): void;
  };
  return { fixture, el: fixture.nativeElement as HTMLElement, store, api, messages, c, failures$, added$ };
}

describe('TasksIndicator', () => {
  it('is hidden while there are no tasks', () => {
    expect(setup([]).el.querySelector('[data-testid="tasks-indicator"]')).toBeNull();
  });

  it('shows how many tasks there are and whether one needs an answer', () => {
    const prompt = { id: 'p1', form: { fields: [] }, expiresAt: '2030-01-01T00:00:00Z' };
    const { el } = setup([task({ id: 'a' }), task({ id: 'b', state: 'waiting', prompt })]);
    const button = el.querySelector('[data-testid="tasks-indicator"]')!;
    expect(button.textContent).toContain('2');
    expect(button.classList).toContain('waiting');
  });

  it('describes each task', () => {
    const { c } = setup([]);
    expect(c.statusText(task({}))).toBe('Extracting');
    expect(c.statusText(task({ status: '' }))).toBe('40%');
    expect(c.statusText(task({ state: 'waiting' }))).toBe('Waiting for an answer');
    expect(c.statusText(task({ state: 'done', message: 'Unpacked 3 files' }))).toBe('Unpacked 3 files');
    expect(c.statusText(task({ state: 'failed', message: null }))).toBe('Failed');
  });

  it('answers, cancels and dismisses', () => {
    const { c, store, api } = setup([]);
    c.answer(task({ id: 'a' }));
    expect(store.request).toHaveBeenCalledWith('a');

    c.stop(task({ id: 'a' }));
    expect(api.cancelOrDismiss).toHaveBeenCalledWith('a');
    expect(store.remove).not.toHaveBeenCalled(); // a running task is only asked to stop

    c.stop(task({ id: 'b', state: 'done' }));
    expect(store.remove).toHaveBeenCalledWith('b');
  });

  it('raises a toast when a task fails', () => {
    const { messages, failures$ } = setup([]);
    failures$.next(task({ state: 'failed', message: 'Bad password' }));
    expect(messages.add).toHaveBeenCalledWith(expect.objectContaining({ severity: 'error', detail: 'Bad password' }));
  });

  it('opens the list when a task is added', async () => {
    const { fixture, el, store, added$ } = setup([task({ id: 'a' })]);
    const show = vi.spyOn(fixture.debugElement.query((d) => d.name === 'p-popover').componentInstance, 'show');
    added$.next(task({ id: 'b' }));
    await new Promise((r) => setTimeout(r));
    expect(show).toHaveBeenCalledWith(expect.anything(), el.querySelector('[data-testid="tasks-indicator"]'));
    expect(store.tasks().length).toBe(1);
  });

  it('closes the list when the last task goes', () => {
    const { fixture, store } = setup([task({ id: 'a' })]);
    const hide = vi.spyOn(fixture.debugElement.query((d) => d.name === 'p-popover').componentInstance, 'hide');
    store.tasks.set([]);
    fixture.detectChanges();
    expect(hide).toHaveBeenCalled();
  });
});
