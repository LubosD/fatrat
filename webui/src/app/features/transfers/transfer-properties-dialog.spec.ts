import { TestBed } from '@angular/core/testing';
import { TransferPropertiesDialog } from './transfer-properties-dialog';
import { Transfer } from '../../core/api/models';

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'file.zip',
    state: 'Active',
    class: 'HttpDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/file.zip',
    dataPathIsDir: false,
    total: 1000,
    done: 250,
    comment: 'existing comment',
    object: '',
    timeRunning: 10,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 10240, up: 5120 },
    ...overrides,
  };
}

describe('TransferPropertiesDialog', () => {
  it('prefills the form from the given transfer', () => {
    TestBed.configureTestingModule({ imports: [TransferPropertiesDialog] });
    const fixture = TestBed.createComponent(TransferPropertiesDialog);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.comment).toBe('existing comment');
    expect(fixture.componentInstance.speedDown).toBe(10);
    expect(fixture.componentInstance.speedUp).toBe(5);
  });

  it('leaves the form blank when transfer is null', () => {
    TestBed.configureTestingModule({ imports: [TransferPropertiesDialog] });
    const fixture = TestBed.createComponent(TransferPropertiesDialog);
    fixture.componentRef.setInput('transfer', null);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.comment).toBe('');
    expect(fixture.componentInstance.speedDown).toBe(0);
  });

  it('emits save() with the current form values and closes', () => {
    TestBed.configureTestingModule({ imports: [TransferPropertiesDialog] });
    const fixture = TestBed.createComponent(TransferPropertiesDialog);
    fixture.componentRef.setInput('transfer', makeTransfer());
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    const saved: unknown[] = [];
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.save.subscribe((value) => saved.push(value));
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance.comment = 'updated comment';
    fixture.componentInstance['onSave']();

    expect(saved).toEqual([{ comment: 'updated comment', speedLimits: { down: 10240, up: 5120 } }]);
    expect(visibleChanges).toEqual([false]);
  });

  it('clears a previously-typed comment when reopened with the same null transfer (e.g. after Cancel)', () => {
    TestBed.configureTestingModule({ imports: [TransferPropertiesDialog] });
    const fixture = TestBed.createComponent(TransferPropertiesDialog);
    fixture.componentRef.setInput('transfer', null);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    fixture.componentInstance.comment = 'typed but not saved';
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.comment).toBe('');
  });

  it('discards unsaved edits when reopened on the same transfer reference', () => {
    TestBed.configureTestingModule({ imports: [TransferPropertiesDialog] });
    const fixture = TestBed.createComponent(TransferPropertiesDialog);
    const transfer = makeTransfer({ comment: 'original' });
    fixture.componentRef.setInput('transfer', transfer);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    fixture.componentInstance.comment = 'edited but not saved';
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.comment).toBe('original');
  });

  describe('data directory', () => {
    function open(transfer: Transfer) {
      TestBed.configureTestingModule({ imports: [TransferPropertiesDialog] });
      const fixture = TestBed.createComponent(TransferPropertiesDialog);
      fixture.componentRef.setInput('transfer', transfer);
      fixture.componentRef.setInput('visible', true);
      fixture.detectChanges();
      const saved: { object?: string }[] = [];
      fixture.componentInstance.save.subscribe((v) => saved.push(v));
      return { component: fixture.componentInstance, saved };
    }

    it('prefills from the transfer and omits object from save() when unchanged', () => {
      const { component, saved } = open(makeTransfer({ object: '/downloads' }));
      expect(component.object).toBe('/downloads');
      component['onSave']();
      expect(saved[0].object).toBeUndefined();
    });

    it('emits the new directory when it was changed', () => {
      const { component, saved } = open(makeTransfer({ object: '/downloads' }));
      component.object = ' /elsewhere ';
      component['onSave']();
      expect(saved[0].object).toBe('/elsewhere');
    });

    it('never emits a directory for upload transfers', () => {
      const { component, saved } = open(makeTransfer({ primaryMode: 'Upload', object: '/src' }));
      component.object = '/elsewhere';
      component['onSave']();
      expect(saved[0].object).toBeUndefined();
    });
  });
});
