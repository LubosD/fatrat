import { TestBed } from '@angular/core/testing';
import { QueuePropertiesDialog } from './queue-properties-dialog';
import { Queue } from '../../core/api/models';

function makeQueue(overrides: Partial<Queue> = {}): Queue {
  return {
    uuid: 'q1',
    name: 'Default',
    defaultDirectory: '/downloads',
    moveDirectory: '',
    upAsDown: false,
    transferLimits: { down: 0, up: 0 },
    speedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

describe('QueuePropertiesDialog', () => {
  it('shows "New Queue" and blank fields when queue is null', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    fixture.componentRef.setInput('queue', null);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.name).toBe('');
    expect(fixture.componentInstance.speedDown).toBe(0);
    expect(fixture.nativeElement.textContent).toContain('New Queue');
  });

  it('shows "Queue Properties" and prefills fields when a queue is given', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    fixture.componentRef.setInput('queue', makeQueue({ name: 'Movies', speedLimits: { down: 100, up: 50 } }));
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.name).toBe('Movies');
    expect(fixture.componentInstance.speedDown).toBe(100);
    expect(fixture.componentInstance.speedUp).toBe(50);
    expect(fixture.nativeElement.textContent).toContain('Queue Properties');
  });

  it('resets the form when the queue input changes to a different queue', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    fixture.componentRef.setInput('queue', makeQueue({ name: 'Movies' }));
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();
    expect(fixture.componentInstance.name).toBe('Movies');

    fixture.componentRef.setInput('queue', makeQueue({ uuid: 'q2', name: 'Music' }));
    fixture.detectChanges();
    expect(fixture.componentInstance.name).toBe('Music');
  });

  it('emits save() with the current form values and closes', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    fixture.componentRef.setInput('queue', null);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    const saved: unknown[] = [];
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.save.subscribe((value) => saved.push(value));
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance.name = 'New Queue Name';
    fixture.componentInstance.speedDown = 200;
    fixture.componentInstance['onSave']();

    expect(saved).toEqual([
      {
        name: 'New Queue Name',
        defaultDirectory: '',
        moveDirectory: '',
        speedLimits: { down: 200, up: 0 },
        transferLimits: { down: 0, up: 0 },
        upAsDown: false,
      },
    ]);
    expect(visibleChanges).toEqual([false]);
  });

  it('closes without emitting save() on cancel', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    fixture.componentRef.setInput('queue', null);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    const saved: unknown[] = [];
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.save.subscribe((value) => saved.push(value));
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance['onCancel']();

    expect(saved).toEqual([]);
    expect(visibleChanges).toEqual([false]);
  });
});
