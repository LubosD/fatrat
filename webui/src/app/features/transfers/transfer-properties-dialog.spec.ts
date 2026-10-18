import { Observable, of, Subject, throwError } from 'rxjs';
import { TestBed } from '@angular/core/testing';
import { TransferPropertiesDialog } from './transfer-properties-dialog';
import { DirectoryListing, Transfer, TransferOptions } from '../../core/api/models';
import { TransferApi } from '../../core/api/transfer-api';
import { ApiError } from '../../core/api/rest-client';
import { TransferActionsService } from '../../core/transfer-actions.service';

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'file.zip',
    state: 'Active',
    class: 'GeneralDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/file.zip',
    dataPathIsDir: false,
    total: 1000,
    done: 250,
    comment: 'existing comment',
    completionCommand: '',
    object: '/downloads',
    timeRunning: 10,
    speeds: { down: 0, up: 0 },
    userSpeedLimits: { down: 10240, up: 5120 },
    ...overrides,
  };
}

const NO_OPTIONS: TransferOptions = { fields: [], values: {} };
const FILE_NAME: TransferOptions = {
  fields: [{ type: 'text', key: 'fileName', title: 'Target file name' }],
  values: { fileName: 'file.zip' },
};

interface Setup {
  transfer?: Transfer | null;
  count?: number;
  options?: Observable<TransferOptions>;
  update?: Observable<void>;
  setOptions?: Observable<TransferOptions>;
  browseDirectories?: () => Observable<DirectoryListing>;
}

function open(setup: Setup = {}) {
  const getTransferOptions = vi.fn(() => setup.options ?? of(NO_OPTIONS));
  const setTransferOptions = vi.fn(() => setup.setOptions ?? of(NO_OPTIONS));
  const updateProperties = vi.fn(() => setup.update ?? of(undefined));
  TestBed.configureTestingModule({
    imports: [TransferPropertiesDialog],
    providers: [
      { provide: TransferApi, useValue: { browseDirectories: vi.fn(setup.browseDirectories ?? (() => of({ path: '/', parent: '', entries: [] }))), getTransferOptions, setTransferOptions } },
      { provide: TransferActionsService, useValue: { updateProperties } },
    ],
  });
  const fixture = TestBed.createComponent(TransferPropertiesDialog);
  fixture.componentRef.setInput('transfer', setup.transfer === undefined ? makeTransfer() : setup.transfer);
  fixture.componentRef.setInput('transferCount', setup.count ?? 1);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
  const visibleChanges: boolean[] = [];
  fixture.componentInstance.visibleChange.subscribe((v) => visibleChanges.push(v));
  const root = () => document.body;
  return { fixture, component: fixture.componentInstance, getTransferOptions, setTransferOptions, updateProperties, visibleChanges, root };
}

const byTestId = (id: string) => document.body.querySelector(`[data-testid="${id}"]`);

describe('TransferPropertiesDialog', () => {
  afterEach(() => document.body.querySelectorAll('.p-dialog-mask').forEach((e) => e.remove()));

  describe('general', () => {
    it('prefills the form from the given transfer', () => {
      const { component } = open({ transfer: makeTransfer({ completionCommand: 'notify %N' }) });
      expect(component.comment).toBe('existing comment');
      expect(component.speedDown).toBe(10);
      expect(component.speedUp).toBe(5);
      expect(component.object).toBe('/downloads');
      expect(component.completionCommand).toBe('notify %N');
    });

    it('leaves the form blank when transfer is null', () => {
      const { component } = open({ transfer: null });
      expect(component.comment).toBe('');
      expect(component.speedDown).toBe(0);
      expect(component.completionCommand).toBe('');
    });

    it('discards unsaved edits when reopened on the same transfer reference', () => {
      const transfer = makeTransfer({ comment: 'original' });
      const { fixture, component } = open({ transfer });
      component.comment = 'edited but not saved';
      fixture.componentRef.setInput('visible', false);
      fixture.detectChanges();
      fixture.componentRef.setInput('visible', true);
      fixture.detectChanges();
      expect(component.comment).toBe('original');
    });

    it('labels the path Destination with a Browse button for a download', () => {
      open();
      expect(document.body.querySelector('label[for="transfer-object"]')!.textContent).toContain('Destination');
      expect(byTestId('transfer-browse-object')).not.toBeNull();
    });

    it('labels the path Source without Browse for an upload', () => {
      open({ transfer: makeTransfer({ primaryMode: 'Upload', mode: 'Upload', object: '/src/file.bin' }) });
      expect(document.body.querySelector('label[for="transfer-object"]')!.textContent).toContain('Source');
      expect(byTestId('transfer-browse-object')).toBeNull();
    });

    it('lists the completion command placeholders', () => {
      open();
      const hint = byTestId('transfer-completion-hint')!.textContent!;
      for (const p of ['%N', '%T', '%D', '%P']) expect(hint).toContain(p);
    });

    it('clears a previously-typed comment when reopened with the same null transfer (e.g. after Cancel)', () => {
      const { fixture, component } = open({ transfer: null });
      component.comment = 'typed but not saved';
      fixture.componentRef.setInput('visible', false);
      fixture.detectChanges();
      fixture.componentRef.setInput('visible', true);
      fixture.detectChanges();
      expect(component.comment).toBe('');
    });

    describe('Enter in the path field', () => {
      function openWithSuggestions() {
        vi.useFakeTimers();
        const ctx = open({
          transfer: makeTransfer({ object: '/downloads' }),
          browseDirectories: () => of({ path: '/', parent: '', entries: [{ name: 'data', path: '/data' }] }),
        });
        const input = document.getElementById('transfer-object') as HTMLInputElement;
        input.focus();
        input.value = '/d';
        input.dispatchEvent(new Event('input', { bubbles: true }));
        vi.advanceTimersByTime(200);
        return { ...ctx, input };
      }
      const enter = (input: HTMLInputElement) =>
        input.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true, cancelable: true }));

      afterEach(() => vi.useRealTimers());

      it('saves when no suggestion is highlighted', () => {
        const { input, updateProperties } = openWithSuggestions();
        enter(input);
        expect(updateProperties).toHaveBeenCalledTimes(1);
      });

      it('completes the highlighted suggestion instead of saving', () => {
        const { input, updateProperties } = openWithSuggestions();
        input.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true, cancelable: true }));
        enter(input);
        expect(updateProperties).not.toHaveBeenCalled();
        expect(input.value).toBe('/data/');
      });
    });
  });

  describe('saving', () => {
    it('sends the comment and speed limits, and closes', () => {
      const { component, updateProperties, visibleChanges } = open();
      component.comment = 'updated comment';
      component['onSave']();
      expect(updateProperties).toHaveBeenCalledWith({ comment: 'updated comment', userSpeedLimits: { down: 10240, up: 5120 } });
      expect(visibleChanges).toEqual([false]);
    });

    it('sends object and completionCommand only when changed', () => {
      const { component, updateProperties } = open();
      component.object = ' /elsewhere ';
      component.completionCommand = 'notify %N';
      component['onSave']();
      expect(updateProperties).toHaveBeenCalledWith(expect.objectContaining({ object: '/elsewhere', completionCommand: 'notify %N' }));
    });

    it('sends a changed upload source', () => {
      const { component, updateProperties } = open({ transfer: makeTransfer({ primaryMode: 'Upload', object: '/src/a' }) });
      component.object = '/src/b';
      component['onSave']();
      expect(updateProperties).toHaveBeenCalledWith(expect.objectContaining({ object: '/src/b' }));
    });

    it('stays open with the server message when the change is refused', () => {
      const { fixture, component, visibleChanges } = open({
        update: throwError(() => new ApiError(422, 'invalid_property', 'Not a readable directory: /nope')),
      });
      component['onSave']();
      fixture.detectChanges();
      expect(visibleChanges).toEqual([]);
      expect(byTestId('transfer-dialog-error')!.textContent).toContain('Not a readable directory: /nope');
      expect((byTestId('transfer-dialog-save') as HTMLButtonElement).disabled).toBe(false);
    });

    it('ignores a second Save while the first is in flight', () => {
      const pending = new Subject<void>();
      const { component, updateProperties } = open({ update: pending });
      component['onSave']();
      component['onSave']();
      expect(updateProperties).toHaveBeenCalledTimes(1);
    });
  });

  describe('options', () => {
    it('loads the options of a single transfer when opened', async () => {
      const { fixture, getTransferOptions } = open({ options: of(FILE_NAME) });
      await fixture.whenStable(); // ngModel writes the input's value asynchronously
      expect(getTransferOptions).toHaveBeenCalledWith('t1');
      expect((document.getElementById('transfer-option-fileName') as HTMLInputElement).value).toBe('file.zip');
    });

    it('shows a loading state until the options arrive', () => {
      const options = new Subject<TransferOptions>();
      const { fixture } = open({ options });
      expect(byTestId('transfer-options-loading')).not.toBeNull();
      options.next(FILE_NAME);
      fixture.detectChanges();
      expect(byTestId('transfer-options-loading')).toBeNull();
      expect(document.getElementById('transfer-option-fileName')).not.toBeNull();
    });

    it('shows no Options section for a class without options', () => {
      open({ options: of(NO_OPTIONS) });
      expect(byTestId('transfer-options')).toBeNull();
    });

    it('shows an error in the section when loading fails, and still saves the rest', () => {
      const { fixture, component, updateProperties } = open({ options: throwError(() => new ApiError(500, 'x', 'boom')) });
      fixture.detectChanges();
      expect(byTestId('transfer-options-error')!.textContent).toContain('boom');
      component['onSave']();
      expect(updateProperties).toHaveBeenCalled();
    });

    it('does not load options for several transfers', () => {
      const { getTransferOptions } = open({ count: 3 });
      expect(getTransferOptions).not.toHaveBeenCalled();
      expect(byTestId('transfer-options')).toBeNull();
    });

    it('sends only the changed options, after the general properties', () => {
      const { component, updateProperties, setTransferOptions } = open({
        options: of({
          fields: [{ type: 'text', key: 'fileName' }, { type: 'checkbox', key: 'flag' }],
          values: { fileName: 'file.zip', flag: false },
        }),
      });
      component['setOption']('fileName', 'renamed.zip');
      component['onSave']();
      expect(updateProperties).toHaveBeenCalled();
      expect(setTransferOptions).toHaveBeenCalledWith('t1', { fileName: 'renamed.zip' });
    });

    it('makes no options call when no option changed', () => {
      const { component, setTransferOptions, visibleChanges } = open({ options: of(FILE_NAME) });
      component['onSave']();
      expect(setTransferOptions).not.toHaveBeenCalled();
      expect(visibleChanges).toEqual([false]);
    });

    it('stays open with the message when an option is refused', () => {
      const { fixture, component, visibleChanges } = open({
        options: of(FILE_NAME),
        setOptions: throwError(() => new ApiError(422, 'invalid_property', 'Invalid file name: ""')),
      });
      component['setOption']('fileName', '');
      component['onSave']();
      fixture.detectChanges();
      expect(visibleChanges).toEqual([]);
      expect(byTestId('transfer-dialog-error')!.textContent).toContain('Invalid file name');
    });

    it('ignores options that arrive after the dialog was reopened on another transfer', () => {
      const first = new Subject<TransferOptions>();
      const { fixture, getTransferOptions } = open({ options: first });
      getTransferOptions.mockReturnValue(of(NO_OPTIONS));
      fixture.componentRef.setInput('transfer', makeTransfer({ uuid: 't2' }));
      fixture.detectChanges();
      first.next(FILE_NAME);
      fixture.detectChanges();
      expect(document.getElementById('transfer-option-fileName')).toBeNull();
    });
  });
});
