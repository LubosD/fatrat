import { TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { of, throwError } from 'rxjs';
import { MessageService } from 'primeng/api';
import { TaskPromptDialog } from './task-prompt-dialog';
import { TasksStore } from '../../state/tasks.store';
import { TasksApi } from '../../core/tasks-api.service';
import { TransferApi } from '../../core/api/transfer-api';
import { ApiError } from '../../core/api/rest-client';
import { PluginTask } from '../../core/api/models';

function asking(id: string, promptId: string, title = 'Who?'): PluginTask {
  return {
    id, plugin: 'p.so', title: 'Task ' + id, transfer: 't1', transferName: 'file.rar', state: 'waiting',
    progress: -1, status: '', message: null,
    prompt: {
      id: promptId,
      expiresAt: new Date(Date.now() + 90_000).toISOString(),
      form: { title, fields: [{ type: 'text', key: 'name', title: 'Name', default: 'x' }] },
    },
  };
}

function setup(tasks: PluginTask[]) {
  const openPrompts = signal(tasks);
  const store = { openPrompts, requested: signal<string | null>(null), markHandled: vi.fn() };
  const api = { answer: vi.fn().mockReturnValue(of({})), dismissPrompt: vi.fn().mockReturnValue(of(undefined)) };
  const messages = { add: vi.fn() };
  TestBed.configureTestingModule({
    imports: [TaskPromptDialog],
    providers: [
      { provide: TasksStore, useValue: store },
      { provide: TasksApi, useValue: api },
      { provide: MessageService, useValue: messages },
      { provide: TransferApi, useValue: { browseDirectories: () => of({ path: '/', parent: '', entries: [] }) } },
    ],
  });
  const fixture = TestBed.createComponent(TaskPromptDialog);
  fixture.detectChanges();
  return { fixture, store, api, messages, comp: fixture.componentInstance as unknown as Record<string, () => unknown> };
}

describe('TaskPromptDialog', () => {
  it('shows the oldest open question with its defaults and countdown', () => {
    const { comp } = setup([asking('a', 'p1', 'First'), asking('b', 'p2', 'Second')]);
    expect((comp['header'] as () => string)()).toBe('First');
    expect(comp['values']()).toEqual({ name: 'x' });
    expect(comp['secondsLeft']() as number).toBeGreaterThan(80);
  });

  it('shows the one the user asked to answer first', () => {
    const { fixture, store, comp } = setup([asking('a', 'p1', 'First'), asking('b', 'p2', 'Second')]);
    store.requested.set('b');
    fixture.detectChanges();
    expect((comp['header'] as () => string)()).toBe('Second');
  });

  it('sends the answer and hides the question', () => {
    const { fixture, store, api } = setup([asking('a', 'p1')]);
    const c = fixture.componentInstance as unknown as { setValue(k: string, v: string): void; submit(): void };
    c.setValue('name', 'Ann');
    c.submit();
    expect(api.answer).toHaveBeenCalledWith('a', 'p1', { name: 'Ann' });
    expect(store.markHandled).toHaveBeenCalledWith('p1');
  });

  it('keeps the question open on an invalid answer', () => {
    const { fixture, store, api, comp } = setup([asking('a', 'p1')]);
    api.answer.mockReturnValue(throwError(() => new ApiError(400, 'invalid_answer', '"name" must be text')));
    (fixture.componentInstance as unknown as { submit(): void }).submit();
    expect(comp['error']()).toBe('"name" must be text');
    expect(store.markHandled).not.toHaveBeenCalled();
  });

  it('gives up quietly on a question answered elsewhere or expired', () => {
    const { fixture, store, api, messages } = setup([asking('a', 'p1')]);
    api.answer.mockReturnValue(throwError(() => new ApiError(410, 'expired', 'The question has expired')));
    (fixture.componentInstance as unknown as { submit(): void }).submit();
    expect(store.markHandled).toHaveBeenCalledWith('p1');
    expect(messages.add).toHaveBeenCalled();
  });

  it('keeps the question open when the answer did not get through', () => {
    const { fixture, store, api, comp } = setup([asking('a', 'p1')]);
    api.answer.mockReturnValue(throwError(() => new ApiError(0, 'network', 'Connection lost')));
    (fixture.componentInstance as unknown as { submit(): void }).submit();
    expect(comp['error']()).toBe('Connection lost');
    expect(store.markHandled).not.toHaveBeenCalled();
  });

  it('dismisses the question on Cancel', () => {
    const { fixture, store, api } = setup([asking('a', 'p1')]);
    (fixture.componentInstance as unknown as { cancel(): void }).cancel();
    expect(api.dismissPrompt).toHaveBeenCalledWith('a', 'p1');
    expect(store.markHandled).toHaveBeenCalledWith('p1');
  });

  it('keeps what the user typed when the task reports progress', () => {
    const { fixture, store, comp } = setup([asking('a', 'p1')]);
    (fixture.componentInstance as unknown as { setValue(k: string, v: string): void }).setValue('name', 'typed');
    store.openPrompts.set([{ ...asking('a', 'p1'), progress: 40 }]);
    fixture.detectChanges();
    expect(comp['values']()).toEqual({ name: 'typed' });
  });
});
