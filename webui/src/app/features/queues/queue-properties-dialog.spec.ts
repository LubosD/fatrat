import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { TransferApi } from '../../core/api/transfer-api';
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
    fixture.componentRef.setInput('queue', makeQueue({ name: 'Movies', speedLimits: { down: 102400, up: 51200 } }));
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
        speedLimits: { down: 204800, up: 0 },
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

  it('clears a previously-typed name when reopened with the same null queue (e.g. after Cancel)', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    fixture.componentRef.setInput('queue', null);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    fixture.componentInstance.name = 'typed but not saved';
    fixture.componentRef.setInput('visible', false); // simulates Cancel closing it
    fixture.detectChanges();

    // Reopen with queue still null (same reference) — this previously failed to reset.
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.name).toBe('');
  });

  it('discards unsaved edits when reopened on the same queue reference', () => {
    TestBed.configureTestingModule({ imports: [QueuePropertiesDialog] });
    const fixture = TestBed.createComponent(QueuePropertiesDialog);
    const queue = makeQueue({ name: 'Movies' });
    fixture.componentRef.setInput('queue', queue);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    fixture.componentInstance.name = 'edited but not saved';
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    // Reopen with the exact same queue object reference — this previously failed to reset.
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.name).toBe('Movies');
  });

  describe('directory picker', () => {
    function setupPicker() {
      const browseDirectories = vi.fn((path?: string) => of({ path: path || '/home/u', parent: '/home', entries: [] }));
      TestBed.configureTestingModule({
        imports: [QueuePropertiesDialog],
        providers: [{ provide: TransferApi, useValue: { browseDirectories } }],
      });
      const fixture = TestBed.createComponent(QueuePropertiesDialog);
      fixture.componentRef.setInput('queue', makeQueue({ defaultDirectory: '/downloads', moveDirectory: '/done' }));
      fixture.componentRef.setInput('visible', true);
      fixture.detectChanges();
      return { fixture, browseDirectories, el: fixture.nativeElement as HTMLElement };
    }

    it('browses from the default directory and fills it in', () => {
      const { fixture, browseDirectories, el } = setupPicker();
      (el.querySelector('[data-testid="queue-browse-default"]') as HTMLElement).click();
      fixture.detectChanges();
      expect(browseDirectories).toHaveBeenCalledWith('/downloads', 'q1', false);

      (el.querySelector('[data-testid="directory-picker-select"]') as HTMLElement).click();
      fixture.detectChanges();
      expect(fixture.componentInstance.defaultDirectory).toBe('/downloads');
      expect(fixture.componentInstance.moveDirectory).toBe('/done');
      expect(el.querySelector('app-directory-picker')).toBeNull();
    });

    it('browses from the move-to directory and fills only that one', () => {
      const { fixture, browseDirectories, el } = setupPicker();
      (el.querySelector('[data-testid="queue-browse-move"]') as HTMLElement).click();
      fixture.detectChanges();
      expect(browseDirectories).toHaveBeenCalledWith('/done', 'q1', false);

      (el.querySelector('[data-testid="directory-picker-select"]') as HTMLElement).click();
      fixture.detectChanges();
      expect(fixture.componentInstance.moveDirectory).toBe('/done');
      expect(fixture.componentInstance.defaultDirectory).toBe('/downloads');
    });
  });
});
