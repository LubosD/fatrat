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
    watchDirectory: '',
    watchStatus: 'off',
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

  for (const [name, init] of [
    ['Ctrl-Enter', { ctrlKey: true }],
    ['Cmd-Enter', { metaKey: true }],
  ] as const) {
    for (const selector of ['#new-transfer-urls', '#new-transfer-target']) {
      it(`${name} in ${selector} adds the transfers`, () => {
        const fixture = setup();
        fixture.componentInstance.urlsText = 'http://example.com/a';
        const el = document.querySelector(selector) as HTMLElement;
        const event = new KeyboardEvent('keydown', { key: 'Enter', bubbles: true, cancelable: true, ...init });
        el.dispatchEvent(event);
        expect(addTransfers).toHaveBeenCalledWith(['http://example.com/a'], expect.anything());
        expect(event.defaultPrevented).toBe(true);
      });
    }
  }

  it('plain Enter in the URLs textarea does not submit', () => {
    const fixture = setup();
    fixture.componentInstance.urlsText = 'http://example.com/a';
    document.querySelector('#new-transfer-urls')!.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
    expect(addTransfers).not.toHaveBeenCalled();
  });

  it('a selected file takes priority over any typed URLs', () => {
    const fixture = setup();
    fixture.componentInstance.urlsText = 'http://example.com/a';
    const file = new File(['data'], 'test.torrent');
    fixture.componentInstance['addFiles']([file]);
    fixture.componentInstance['onSave']();

    expect(addTransferWithData).toHaveBeenCalledWith(file, expect.objectContaining({ upload: false, queueUuid: 'q1' }));
    expect(addTransfers).not.toHaveBeenCalled();
  });

  it('uploads every chosen file, one request each, and closes once all were accepted', () => {
    const fixture = setup();
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));
    const a = new File(['a'], 'a.torrent');
    const b = new File(['b'], 'b.torrent');
    fixture.componentInstance['addFiles']([a, b]);
    fixture.componentInstance['onSave']();

    expect(addTransferWithData.mock.calls.map((c) => c[0])).toEqual([a, b]);
    expect(visibleChanges).toEqual([false]);
  });

  it('keeps only the failed files listed and names them in the error', () => {
    const a = new File(['a'], 'a.torrent');
    const b = new File(['b'], 'b.torrent');
    const c = new File(['c'], 'c.torrent');
    addTransferWithData.mockImplementation((file: File) => (file === b ? throwError(() => new Error('Invalid torrent')) : of(undefined)));
    const fixture = setup();
    const visibleChanges: boolean[] = [];
    fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));
    fixture.componentInstance['addFiles']([a, b, c]);
    fixture.componentInstance['onSave']();
    fixture.detectChanges();

    // A failure doesn't stop the files after it.
    expect(addTransferWithData).toHaveBeenCalledTimes(3);
    expect(visibleChanges).toEqual([]);
    expect(fixture.componentInstance.files).toEqual([b]);
    expect(fixture.componentInstance['error']()).toBe('b.torrent: Invalid torrent');

    // A retry sends only the file that failed.
    addTransferWithData.mockReset().mockReturnValue(of(undefined));
    fixture.componentInstance['onSave']();
    expect(addTransferWithData.mock.calls.map((call) => call[0])).toEqual([b]);
    expect(visibleChanges).toEqual([false]);
  });

  it('lists the chosen files, skips ones already listed, and can remove one', () => {
    const fixture = setup();
    const a = new File(['a'], 'a.torrent');
    const b = new File(['b'], 'b.torrent');
    fixture.componentInstance['addFiles']([a]);
    fixture.componentInstance['addFiles']([new File(['a'], 'a.torrent', { lastModified: a.lastModified }), b]);
    fixture.detectChanges();

    const names = () => Array.from(fixture.nativeElement.querySelectorAll('[data-testid="new-transfer-file-list"] .file-name')).map((el) => (el as HTMLElement).textContent);
    expect(names()).toEqual(['a.torrent', 'b.torrent']);

    (fixture.nativeElement.querySelectorAll('[data-testid="new-transfer-remove-file"]')[0] as HTMLButtonElement).click();
    fixture.detectChanges();
    expect(names()).toEqual(['b.torrent']);
  });

  it('takes the files picked in the file input, which accepts several', () => {
    const fixture = setup();
    const input = fixture.nativeElement.querySelector('[data-testid="new-transfer-file-input"]') as HTMLInputElement;
    expect(input.multiple).toBe(true);
    const a = new File(['a'], 'a.torrent');
    const b = new File(['b'], 'b.torrent');
    Object.defineProperty(input, 'files', { value: [a, b], configurable: true });
    input.dispatchEvent(new Event('change'));
    expect(fixture.componentInstance.files).toEqual([a, b]);
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
    fixture.componentInstance['addFiles']([new File(['data'], 'test.torrent')]);

    const textarea = fixture.nativeElement.querySelector('#new-transfer-urls');
    expect(textarea.disabled).toBe(true);
  });

  it('onClearFiles() clears the selected files and re-enables urls', () => {
    const fixture = setup();
    fixture.componentInstance['addFiles']([new File(['data'], 'test.torrent')]);
    expect(fixture.componentInstance.files.length).toBe(1);

    fixture.componentInstance['onClearFiles']();

    expect(fixture.componentInstance.files).toEqual([]);
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

  describe('prefillUrls', () => {
    function reopenWith(fixture: ReturnType<typeof setup>, urls: string[]) {
      fixture.componentRef.setInput('visible', false);
      fixture.detectChanges();
      fixture.componentRef.setInput('prefillUrls', urls);
      fixture.componentRef.setInput('visible', true);
      fixture.detectChanges();
    }

    it('fills the URLs when the dialog opens', () => {
      const fixture = setup();
      reopenWith(fixture, ['http://a/1', 'http://b/2']);
      expect(fixture.componentInstance.urlsText).toBe('http://a/1\nhttp://b/2');
    });

    it('does not carry a previous prefill into a plain reopen', () => {
      const fixture = setup();
      reopenWith(fixture, ['http://a/1']);
      reopenWith(fixture, []);
      expect(fixture.componentInstance.urlsText).toBe('');
    });

    it('appends new links to an open dialog, skipping ones already listed', () => {
      const fixture = setup();
      fixture.componentInstance.urlsText = 'http://a/1\n';
      fixture.componentRef.setInput('prefillUrls', ['http://a/1', 'http://c/3']);
      fixture.detectChanges();
      expect(fixture.componentInstance.urlsText).toBe('http://a/1\nhttp://c/3');
    });

    it('drops a chosen file when a link arrives, so the link is not silently ignored', () => {
      const fixture = setup();
      fixture.componentInstance.files = [new File(['x'], 'a.torrent')];
      fixture.componentRef.setInput('prefillUrls', ['http://c/3']);
      fixture.detectChanges();
      expect(fixture.componentInstance.files).toEqual([]);
      expect(fixture.componentInstance.urlsText).toBe('http://c/3');
    });

    describe('while a request is in flight', () => {
      it('keeps a link that arrives during a successful add, and stays open with just that link', () => {
        const pending = new Subject<void>();
        addTransfers.mockReturnValue(pending);
        const fixture = setup();
        const visibleChanges: boolean[] = [];
        fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));
        fixture.componentInstance.urlsText = 'http://a/1';
        fixture.componentInstance['onSave']();

        fixture.componentRef.setInput('prefillUrls', ['http://c/3']);
        fixture.detectChanges();
        expect(fixture.componentInstance.urlsText).toBe('http://a/1');

        pending.next();
        pending.complete();
        fixture.detectChanges();

        expect(visibleChanges).toEqual([]);
        expect(fixture.componentInstance.visible).toBe(true);
        expect(fixture.componentInstance.urlsText).toBe('http://c/3');
        expect(fixture.componentInstance['saving']()).toBe(false);
      });

      it('appends a link that arrived during a failed add to the original lines', () => {
        const pending = new Subject<void>();
        addTransfers.mockReturnValue(pending);
        const fixture = setup();
        fixture.componentInstance.urlsText = 'http://a/1\nhttp://b/2';
        fixture.componentInstance['onSave']();

        fixture.componentRef.setInput('prefillUrls', ['http://c/3']);
        fixture.detectChanges();
        pending.error(new Error('server said no'));
        fixture.detectChanges();

        expect(fixture.componentInstance.urlsText).toBe('http://a/1\nhttp://b/2\nhttp://c/3');
        expect(fixture.componentInstance['error']()).toBe('server said no');
        expect(fixture.componentInstance.visible).toBe(true);
      });

      it('does not discard a file whose upload is in flight when a link arrives', () => {
        const pending = new Subject<void>();
        addTransferWithData.mockReturnValue(pending);
        const fixture = setup();
        const file = new File(['x'], 'a.torrent');
        fixture.componentInstance.files = [file];
        fixture.componentInstance['onSave']();

        fixture.componentRef.setInput('prefillUrls', ['http://c/3']);
        fixture.detectChanges();

        expect(fixture.componentInstance.files).toEqual([file]);
      });

      it('does not leak a queued link into a later session', () => {
        const pending = new Subject<void>();
        addTransfers.mockReturnValue(pending);
        const fixture = setup();
        fixture.componentInstance.urlsText = 'http://a/1';
        fixture.componentInstance['onSave']();
        fixture.componentRef.setInput('prefillUrls', ['http://c/3']);
        fixture.detectChanges();

        reopenWith(fixture, []);
        pending.next();
        pending.complete();
        expect(fixture.componentInstance.urlsText).toBe('');
      });
    });
  });
});
