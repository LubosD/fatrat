import { TestBed } from '@angular/core/testing';
import { NewTransferDialog } from './new-transfer-dialog';
import { Queue, TransferClass } from '../../core/api/models';

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

describe('NewTransferDialog', () => {
  function setup(queues: Queue[] = [makeQueue()], transferClasses: TransferClass[] = [], defaultQueueUuid: string | null = null) {
    TestBed.configureTestingModule({ imports: [NewTransferDialog] });
    const fixture = TestBed.createComponent(NewTransferDialog);
    fixture.componentRef.setInput('queues', queues);
    fixture.componentRef.setInput('transferClasses', transferClasses);
    fixture.componentRef.setInput('defaultQueueUuid', defaultQueueUuid);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();
    return fixture;
  }

  it('defaults the destination queue to defaultQueueUuid when given', () => {
    const fixture = setup([makeQueue({ uuid: 'q1' }), makeQueue({ uuid: 'q2' })], [], 'q2');
    expect(fixture.componentInstance.queueUuid).toBe('q2');
  });

  it('defaults the destination queue to the first queue when defaultQueueUuid is null', () => {
    const fixture = setup([makeQueue({ uuid: 'q1' }), makeQueue({ uuid: 'q2' })], [], null);
    expect(fixture.componentInstance.queueUuid).toBe('q1');
  });

  it('resets the form to defaults when reopened (e.g. after Cancel)', () => {
    const fixture = setup([makeQueue({ uuid: 'q1' })]);
    fixture.componentInstance.urlsText = 'http://example.com/typed-but-not-saved';
    fixture.componentInstance.target = '/tmp';
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance.urlsText).toBe('');
    expect(fixture.componentInstance.target).toBe('');
  });

  it('emits save() with urls split from the textarea, trimmed, and empty lines dropped', () => {
    const fixture = setup();
    const saved: unknown[] = [];
    fixture.componentInstance.save.subscribe((value) => saved.push(value));

    fixture.componentInstance.urlsText = '  http://example.com/a  \n\nhttp://example.com/b\n';
    fixture.componentInstance.queueUuid = 'q1';
    fixture.componentInstance.target = '/downloads';
    fixture.componentInstance['onSave']();

    expect(saved).toEqual([
      {
        urls: ['http://example.com/a', 'http://example.com/b'],
        file: null,
        queueUuid: 'q1',
        targetClass: '',
        target: '/downloads',
        paused: false,
        speedLimits: { down: 0, up: 0 },
      },
    ]);
  });

  it('a selected file takes priority over any typed URLs', () => {
    const fixture = setup();
    const saved: unknown[] = [];
    fixture.componentInstance.save.subscribe((value) => saved.push(value));

    fixture.componentInstance.urlsText = 'http://example.com/a';
    const file = new File(['data'], 'test.torrent');
    fixture.componentInstance['onFileSelect']({ files: [file] } as unknown as import('primeng/fileupload').FileSelectEvent);
    fixture.componentInstance['onSave']();

    expect((saved[0] as { file: File | null }).file).toBe(file);
    expect((saved[0] as { urls: string[] }).urls).toEqual([]);
  });

  it('closes without emitting save() on cancel', () => {
    const fixture = setup();
    const saved: unknown[] = [];
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.save.subscribe((value) => saved.push(value));
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance['onCancel']();

    expect(saved).toEqual([]);
    expect(visibleChanges).toEqual([false]);
  });
});
