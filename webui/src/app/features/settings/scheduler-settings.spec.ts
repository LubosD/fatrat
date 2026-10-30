import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { signal } from '@angular/core';
import { ConfirmationService } from '@openng/optimus-ui/api';
import { SchedulerSettings } from './scheduler-settings';
import { SchedulerApi } from '../../core/scheduler-api.service';
import { ApiError } from '../../core/api/rest-client';
import { QueuesStore } from '../../state/queues.store';
import { ViewportService } from '../../core/viewport.service';
import { Queue, ScheduledAction } from '../../core/api/models';

const action: ScheduledAction = {
  id: 'a1', name: 'Night', queue: 'q1', action: 'stop', repeated: true,
  time: '03:00', days: [true, true, true, true, true, false, false], at: '',
};
const queue = { uuid: 'q1', name: 'Main' } as Queue;

function configure(opts: { mobile?: boolean; actions?: ScheduledAction[]; queues?: Queue[]; api?: Partial<SchedulerApi> } = {}) {
  const confirm = vi.fn();
  const api = { list: vi.fn().mockReturnValue(of(opts.actions ?? [action])), ...opts.api };
  TestBed.configureTestingModule({
    imports: [SchedulerSettings],
    providers: [
      { provide: SchedulerApi, useValue: api },
      { provide: QueuesStore, useValue: { queues: signal(opts.queues ?? [queue]) } },
      { provide: ConfirmationService, useValue: { confirm } },
      { provide: ViewportService, useValue: { isMobile: signal(opts.mobile ?? false) } },
    ],
  });
  const fixture = TestBed.createComponent(SchedulerSettings);
  fixture.detectChanges();
  return { fixture, c: fixture.componentInstance as unknown as Record<string, any>, api, confirm };
}

const apiError = (message: string) => throwError(() => ({ message }) as ApiError);

describe('SchedulerSettings', () => {
  it('loads and lists the actions with their queue, action and schedule', () => {
    const { fixture, c } = configure();
    expect(c['actions']()).toEqual([action]);
    const text = fixture.nativeElement.querySelector('[data-testid="sched-table"]').textContent;
    expect(text).toContain('Night');
    expect(text).toContain('Main');
    expect(text).toContain('Stop all');
    expect(text).toContain('Mon–Fri 03:00');
  });

  it('lists the actions as cards on a phone', () => {
    const { fixture } = configure({ mobile: true });
    expect(fixture.nativeElement.querySelector('[data-testid="sched-table"]')).toBeNull();
    expect(fixture.nativeElement.querySelector('[data-testid="sched-list"]').textContent).toContain('Mon–Fri 03:00');
  });

  it('says so when there are none', () => {
    const { fixture } = configure({ actions: [] });
    expect(fixture.nativeElement.textContent).toContain('No scheduled actions yet.');
  });

  it('names a deleted queue instead of leaving the cell blank', () => {
    const { c } = configure({ actions: [{ ...action, queue: 'gone' }] });
    expect(c['queueName']('gone')).toBe('(deleted queue)');
  });

  it('cannot add an action without a queue', () => {
    const { c } = configure({ queues: [] });
    expect(c['canAdd']()).toBe(false);
    c['add']();
    expect(c['dialogOpen']()).toBe(false);
  });

  it('creates an action, closes the dialog and reloads', () => {
    const create = vi.fn().mockReturnValue(of(action));
    const { c, api } = configure({ api: { create } });
    c['add']();
    expect(c['dialogOpen']()).toBe(true);
    c['saveAction']({ name: 'N' });
    expect(create).toHaveBeenCalledWith({ name: 'N' });
    expect(c['dialogOpen']()).toBe(false);
    expect(api.list).toHaveBeenCalledTimes(2);
  });

  it('updates the edited action by id', () => {
    const update = vi.fn().mockReturnValue(of(action));
    const { c } = configure({ api: { update } });
    c['edit'](action);
    c['saveAction']({ name: 'M' });
    expect(update).toHaveBeenCalledWith('a1', { name: 'M' });
  });

  it('keeps the dialog open and shows the server message when saving fails', () => {
    const { c } = configure({ api: { create: vi.fn().mockReturnValue(apiError('bad queue')) } });
    c['add']();
    c['saveAction']({ name: 'N' });
    expect(c['dialogOpen']()).toBe(true);
    expect(c['dialogError']()).toBe('bad queue');
  });

  it('deletes only after confirmation', () => {
    const del = vi.fn().mockReturnValue(of(undefined));
    const { c, confirm, api } = configure({ api: { delete: del } });
    c['remove'](action);
    expect(del).not.toHaveBeenCalled();
    expect(confirm.mock.calls[0][0].message).toContain('"Night"');

    confirm.mock.calls[0][0].accept();
    expect(del).toHaveBeenCalledWith('a1');
    expect(api.list).toHaveBeenCalledTimes(2);
  });

  it('shows a failed delete', () => {
    const { c, confirm } = configure({ api: { delete: vi.fn().mockReturnValue(apiError('nope')) } });
    c['remove'](action);
    confirm.mock.calls[0][0].accept();
    expect(c['message']()).toBe('nope');
  });
});
