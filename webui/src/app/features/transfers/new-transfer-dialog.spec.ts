import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { TransferApi } from '../../core/api/transfer-api';
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
  const browseDirectories = vi.fn((path?: string) =>
    of({ path: path || '/home/u', parent: '/home', entries: [{ name: 'movies', path: '/home/u/movies' }] }),
  );

  beforeEach(() => browseDirectories.mockClear());

  function setup(queues: Queue[] = [makeQueue()], transferClasses: TransferClass[] = [], defaultQueueUuid: string | null = null) {
    TestBed.configureTestingModule({
      imports: [NewTransferDialog],
      providers: [{ provide: TransferApi, useValue: { browseDirectories: browseDirectories } }],
    });
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
    expect(fixture.componentInstance.target).toBe('/downloads');
  });

  it('pre-fills the target with the selected queue default directory and follows queue changes', () => {
    const fixture = setup([makeQueue({ uuid: 'q1', defaultDirectory: '/a' }), makeQueue({ uuid: 'q2', defaultDirectory: '/b' })]);
    expect(fixture.componentInstance.target).toBe('/a');
    fixture.componentInstance['onQueueChange']('q2');
    expect(fixture.componentInstance.target).toBe('/b');
    fixture.componentInstance.target = '/custom';
    fixture.componentInstance['onQueueChange']('q1');
    expect(fixture.componentInstance.target).toBe('/custom');
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

  it('disables the URLs textarea once a file is selected', () => {
    const fixture = setup();
    fixture.componentInstance['onFileSelect']({ files: [new File(['data'], 'test.torrent')] } as unknown as import('primeng/fileupload').FileSelectEvent);

    const textarea = fixture.nativeElement.querySelector('#new-transfer-urls');
    expect(textarea.disabled).toBe(true);
  });

  it('onClearFile() clears the selected file and re-enables urls', () => {
    const fixture = setup();
    fixture.componentInstance['onFileSelect']({ files: [new File(['data'], 'test.torrent')] } as unknown as import('primeng/fileupload').FileSelectEvent);
    expect(fixture.componentInstance.file).not.toBeNull();

    fixture.componentInstance['onClearFile']();

    expect(fixture.componentInstance.file).toBeNull();
    const textarea = fixture.nativeElement.querySelector('#new-transfer-urls');
    expect(textarea.disabled).toBe(false);
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

  it('Browse opens the directory picker seeded with the typed target and selected queue', () => {
    const fixture = setup([makeQueue({ uuid: 'q1' })]);
    fixture.componentInstance.target = '/mnt/x';
    const el = fixture.nativeElement as HTMLElement;
    (el.querySelector('[data-testid="new-transfer-browse"]') as HTMLElement).click();
    fixture.detectChanges();

    expect(browseDirectories).toHaveBeenCalledWith('/mnt/x', 'q1', false);
    expect(el.querySelector('app-directory-picker')).not.toBeNull();
  });

  it('puts the directory chosen in the picker into the target field', () => {
    const fixture = setup();
    fixture.componentInstance.target = '';
    const el = fixture.nativeElement as HTMLElement;
    (el.querySelector('[data-testid="new-transfer-browse"]') as HTMLElement).click();
    fixture.detectChanges();
    (el.querySelector('[data-testid="directory-picker-select"]') as HTMLElement).click();
    fixture.detectChanges();

    expect(fixture.componentInstance.target).toBe('/home/u');
    expect(el.querySelector('app-directory-picker')).toBeNull();
  });
});
