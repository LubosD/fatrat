import { ScheduledAction } from '../../core/api/models';
import { actionLabel, daysLabel, nowLocal, toBytes, toKiB, whenLabel } from './scheduler-format';

const base: ScheduledAction = {
  id: 'a', name: 'n', queue: 'q', action: 'resume', repeated: true,
  time: '03:00', days: [true, true, true, true, true, false, false], at: '',
};

describe('scheduler-format', () => {
  it('summarises the days', () => {
    expect(daysLabel([true, true, true, true, true, true, true])).toBe('Every day');
    expect(daysLabel([true, true, true, true, true, false, false])).toBe('Mon–Fri');
    expect(daysLabel([true, false, true, false, false, false, true])).toBe('Mon, Wed, Sun');
    expect(daysLabel([true, true, false, false, false, true, true])).toBe('Mon, Tue, Sat, Sun');
    expect(daysLabel([false, false, false, true, true, true, false])).toBe('Thu–Sat');
    expect(daysLabel([false, false, false, false, false, false, false])).toBe('');
  });

  it('describes when a repeated and a one-time action runs', () => {
    expect(whenLabel(base)).toBe('Mon–Fri 03:00');
    expect(whenLabel({ ...base, repeated: false, at: '2026-10-20T03:00' })).toBe('2026-10-20 03:00');
  });

  it('converts limits between bytes/s and kB/s', () => {
    expect(toKiB(51200)).toBe(50);
    expect(toBytes(50)).toBe(51200);
    expect(toBytes(0)).toBe(0);
  });

  it('describes the action, spelling out the limits', () => {
    expect(actionLabel(base)).toBe('Resume all');
    expect(actionLabel({ ...base, action: 'stop' })).toBe('Stop all');
    expect(actionLabel({ ...base, action: 'speedLimit', speedLimit: { down: 51200, up: 0 } }))
      .toBe('Set speed limit (down 50 kB/s, up unlimited)');
  });

  it('formats the current time for a datetime-local input', () => {
    expect(nowLocal()).toMatch(/^\d{4}-\d\d-\d\dT\d\d:\d\d$/);
  });
});
