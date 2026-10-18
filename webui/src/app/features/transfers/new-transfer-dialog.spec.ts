import { TestBed } from '@angular/core/testing';
import { Subject, of, throwError } from 'rxjs';
import { TransferActionsService } from '../../core/transfer-actions.service';
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

  const addTransfers = vi.fn();
  const addTransferWithData = vi.fn();

  beforeEach(() => {
    browseDirectories.mockClear();
    addTransfers.mockReset().mockReturnValue(of(undefined));
    addTransferWithData.mockReset().mockReturnValue(of(undefined));
  });

  function setup(queues: Queue[] = [makeQueue()], transferClasses: TransferClass[] = [], defaultQueueUuid: string | null = null) {
    TestBed.configureTestingModule({
      imports: [NewTransferDialog],
      providers: [
        { provide: TransferApi, useValue: { browseDirectories: browseDirectories } },
        { provide: TransferActionsService, useValue: { addTransfers, addTransferWithData } },
      ],
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

  it('adds the URLs split from the textarea, trimmed, and empty lines dropped, then closes', () => {
    const fixture = setup();
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance.urlsText = '  http://example.com/a  \n\nmagnet:?xt=urn:btih:abc\n';
    fixture.componentInstance.queueUuid = 'q1';
    fixture.componentInstance.target = '/downloads';
    fixture.componentInstance['onSave']();

    expect(addTransfers).toHaveBeenCalledWith(['http://example.com/a', 'magnet:?xt=urn:btih:abc'], {
      upload: false,
      queueUuid: 'q1',
      targetClass: '',
      target: '/downloads',
      paused: false,
      speedLimits: { down: 0, up: 0 },
    });
    expect(visibleChanges).toEqual([false]);
  });

  it('a selected file takes priority over any typed URLs', () => {
    const fixture = setup();
    fixture.componentInstance.urlsText = 'http://example.com/a';
    const file = new File(['data'], 'test.torrent');
    fixture.componentInstance['onFileSelect']({ files: [file] } as unknown as import('primeng/fileupload').FileSelectEvent);
    fixture.componentInstance['onSave']();

    expect(addTransferWithData).toHaveBeenCalledWith(file, expect.objectContaining({ upload: false, queueUuid: 'q1' }));
    expect(addTransfers).not.toHaveBeenCalled();
  });

  it('stays open, shows the server message and keeps the input when adding fails', async () => {
    addTransfers.mockReturnValue(throwError(() => new Error('Unsupported protocol: "magnet"')));
    const fixture = setup();
    await fixture.whenStable();
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance.urlsText = 'http://example.com/a\nmagnet:?xt=urn:btih:abc';
    fixture.componentInstance.target = '/mnt/x';
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.componentInstance['onSave']();
    fixture.detectChanges();

    expect(visibleChanges).toEqual([]);
    expect(fixture.componentInstance.urlsText).toBe('http://example.com/a\nmagnet:?xt=urn:btih:abc');
    expect(fixture.componentInstance.target).toBe('/mnt/x');
    const alert = document.body.querySelector('[data-testid="new-transfer-error"]');
    expect(alert?.getAttribute('role')).toBe('alert');
    expect(alert?.textContent).toContain('Unsupported protocol: "magnet"');

    // A retry clears the message and can succeed.
    addTransfers.mockReturnValue(of(undefined));
    fixture.componentInstance['onSave']();
    expect(visibleChanges).toEqual([false]);
    expect(fixture.componentInstance['error']()).toBe('');
  });

  it('asks for a URL or file instead of sending an empty request', () => {
    const fixture = setup();
    fixture.componentInstance['onSave']();
    fixture.detectChanges();

    expect(addTransfers).not.toHaveBeenCalled();
    expect(document.body.querySelector('[data-testid="new-transfer-error"]')).not.toBeNull();
  });

  it('ignores a second Save while the request is in flight', () => {
    const pending = new Subject<void>();
    addTransfers.mockReturnValue(pending);
    const fixture = setup();
    fixture.componentInstance.urlsText = 'http://example.com/a';

    fixture.componentInstance['onSave']();
    fixture.componentInstance['onSave']();
    expect(addTransfers).toHaveBeenCalledTimes(1);

    pending.next();
    pending.complete();
    fixture.componentInstance.urlsText = 'http://example.com/b';
    fixture.componentInstance['onSave']();
    expect(addTransfers).toHaveBeenCalledTimes(2);
  });

  it('drops the result of a request started in an earlier session', () => {
    const pending = new Subject<void>();
    addTransfers.mockReturnValue(pending);
    const fixture = setup();
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));
    fixture.componentInstance.urlsText = 'http://example.com/a';
    fixture.componentInstance['onSave']();

    // Closed and reopened while the request was still running.
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    pending.error(new Error('late failure'));
    fixture.detectChanges();

    expect(fixture.componentInstance['error']()).toBe('');
    expect(visibleChanges).toEqual([]);
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

  it('closes without adding anything on cancel', () => {
    const fixture = setup();
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));

    fixture.componentInstance['onCancel']();

    expect(addTransfers).not.toHaveBeenCalled();
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
