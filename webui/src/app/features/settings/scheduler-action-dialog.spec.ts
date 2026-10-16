import { TestBed } from '@angular/core/testing';
import { SchedulerActionDialog } from './scheduler-action-dialog';
import { Queue, ScheduledAction } from '../../core/api/models';

const queues = [
  { uuid: '{q1}', name: 'Main' },
  { uuid: '{q2}', name: 'Night' },
] as Queue[];

const existing: ScheduledAction = {
  id: 'a1', name: 'Slow', queue: '{q2}', action: 'speedLimit', speedLimit: { down: 51200, up: 0 },
  repeated: true, time: '23:30', days: [true, true, true, true, true, false, false], at: '',
};

function create(action: ScheduledAction | null) {
  TestBed.configureTestingModule({ imports: [SchedulerActionDialog] });
  const fixture = TestBed.createComponent(SchedulerActionDialog);
  fixture.componentRef.setInput('action', action);
  fixture.componentRef.setInput('queues', queues);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
  const save = vi.fn();
  fixture.componentInstance.save.subscribe(save);
  return { fixture, c: fixture.componentInstance as unknown as Record<string, any>, save };
}

describe('SchedulerActionDialog', () => {
  it('starts with sensible defaults for a new action', () => {
    const { fixture, c } = create(null);
    expect(fixture.nativeElement.textContent).toContain('Add scheduled action');
    expect(c['name']).toBe('');
    expect(c['queue']).toBe('{q1}');
    expect(c['type']).toBe('resume');
    expect(c['repeated']).toBe(true);
    expect(c['days']).toEqual(Array(7).fill(true));
    expect(c['time']).toMatch(/^\d\d:\d\d$/);
  });

  it('is prefilled when editing, showing the limits in kB/s', () => {
    const { fixture, c } = create(existing);
    expect(fixture.nativeElement.textContent).toContain('Edit scheduled action');
    expect(c['name']).toBe('Slow');
    expect(c['queue']).toBe('{q2}');
    expect(c['type']).toBe('speedLimit');
    expect(c['down']).toBe(50);
    expect(c['up']).toBe(0);
    expect(c['time']).toBe('23:30');
    expect(c['days'][5]).toBe(false);
  });

  it('does not emit save while the form is invalid', () => {
    const { c, save } = create(null);
    c['onSave']();
    expect(save).not.toHaveBeenCalled();

    c['name'] = 'x';
    c['days'] = Array(7).fill(false);
    c['onSave']();
    expect(save).not.toHaveBeenCalled();

    c['repeated'] = false;
    c['at'] = '';
    c['onSave']();
    expect(save).not.toHaveBeenCalled();
  });

  it('emits a repeated action with its time and days, and without the limits of another action', () => {
    const { c, save } = create(null);
    c['name'] = ' Wake ';
    c['time'] = '07:05';
    c['days'] = [true, false, true, false, true, false, true];
    c['onSave']();
    expect(save).toHaveBeenCalledWith({
      name: 'Wake', queue: '{q1}', action: 'resume', repeated: true,
      time: '07:05', days: [true, false, true, false, true, false, true],
    });
  });

  it('emits the limits in bytes/s', () => {
    const { c, save } = create(null);
    c['name'] = 'Slow';
    c['type'] = 'speedLimit';
    c['down'] = 50;
    c['up'] = null;
    c['onSave']();
    expect(save.mock.calls[0][0].speedLimit).toEqual({ down: 51200, up: 0 });
  });

  it('emits a one-time action with its date and no weekdays', () => {
    const { c, save } = create(null);
    c['name'] = 'Once';
    c['repeated'] = false;
    c['at'] = '2026-10-20T03:00';
    c['onSave']();
    expect(save).toHaveBeenCalledWith({
      name: 'Once', queue: '{q1}', action: 'resume', repeated: false, at: '2026-10-20T03:00',
    });
  });

  it('stays open on save so the parent can report an error, and shows it', () => {
    const { fixture, c } = create(null);
    const visibleChange = vi.fn();
    fixture.componentInstance.visibleChange.subscribe(visibleChange);
    c['name'] = 'x';
    c['onSave']();
    expect(visibleChange).not.toHaveBeenCalled();

    fixture.componentRef.setInput('error', 'Property "queue" does not name an existing queue');
    fixture.detectChanges();
    expect(document.body.textContent).toContain('does not name an existing queue');
  });

  it('falls back to the first queue if the action refers to a deleted one', () => {
    const { c } = create({ ...existing, queue: '{gone}' });
    expect(c['queue']).toBe('{q1}');
  });
});
