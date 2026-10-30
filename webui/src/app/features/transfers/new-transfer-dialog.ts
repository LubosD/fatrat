import {
  ChangeDetectorRef,
  Component,
  signal,
  ElementRef,
  EventEmitter,
  inject,
  Input,
  OnChanges,
  Output,
  SimpleChanges,
  ViewChild,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule, NgModel } from '@angular/forms';
import { Observable, catchError, concatMap, from, map, of, tap, toArray } from 'rxjs';
import { Dialog } from '@openng/optimus-ui/dialog';
import { Textarea } from '@openng/optimus-ui/textarea';
import { InputText } from '@openng/optimus-ui/inputtext';
import { InputNumber } from '@openng/optimus-ui/inputnumber';
import { Checkbox } from '@openng/optimus-ui/checkbox';
import { Select } from '@openng/optimus-ui/select';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { InputGroup } from '@openng/optimus-ui/inputgroup';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { NewTransferOptions, Queue, TransferClass } from '../../core/api/models';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { PathSuggest } from '../../shared/path-suggest/path-suggest';

@Component({
  selector: 'app-new-transfer-dialog',
  imports: [
    FormsModule,
    Dialog,
    Textarea,
    InputText,
    InputNumber,
    Checkbox,
    Select,
    ButtonDirective,
    InputGroup,
    DirectoryPicker,
    PathSuggest,
  ],
  templateUrl: './new-transfer-dialog.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './new-transfer-dialog.scss',
})
/**
 * Adds the transfers itself: on a failure it stays open with the server's message and the typed
 * input, and it closes only once the server accepted everything.
 */
export class NewTransferDialog implements OnChanges {
  private readonly actions = inject(TransferActionsService);
  private readonly cdr = inject(ChangeDetectorRef);
  @Input() queues: Queue[] = [];
  @Input() transferClasses: TransferClass[] = [];
  @Input() defaultQueueUuid: string | null = null;
  /** Links to put in the URL box: on opening, or appended while the dialog is already open. */
  @Input() prefillUrls: string[] = [];
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();

  @ViewChild('urlsModel') private urlsModel?: NgModel;
  @ViewChild('urlsModel', { read: ElementRef }) private urlsEl?: ElementRef<HTMLTextAreaElement>;

  urlsText = '';
  files: File[] = [];
  queueUuid = '';
  targetClass = '';
  target = '';
  paused = false;
  protected pickerOpen = false;
  speedDown = 0;
  speedUp = 0;
  protected readonly error = signal('');
  protected readonly saving = signal(false);
  // Bumped whenever the form is reset, so the answer to a request from an earlier session is dropped.
  private session = 0;
  // Links that arrived while an Add was in flight; the form is left alone until that Add settles.
  private queuedUrls: string[] = [];

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if (
      changes['visible']?.currentValue === true ||
      (!this.visible && ('defaultQueueUuid' in changes || 'queues' in changes))
    ) {
      this.resetForm();
    } else if (this.visible && !this.queueUuid) {
      // Background polling hands us a fresh `queues` array every refresh; while the dialog is open
      // that must not reset the form or close the directory picker — only fill in a missing queue.
      this.queueUuid = this.defaultQueueUuid ?? this.queues[0]?.uuid ?? '';
      this.target = this.queueDefaultDir(this.queueUuid);
    }
    // After a reset this fills the empty box; while open it appends.
    if (this.visible && ('prefillUrls' in changes || changes['visible']?.currentValue === true)) {
      this.appendUrls(this.prefillUrls);
    }
  }

  private appendUrls(urls: string[]): void {
    if (this.saving()) {
      this.queuedUrls.push(...urls);
      return;
    }
    const listed = new Set(this.urlsText.split('\n').map((line) => line.trim()));
    const added = urls.filter((url) => !listed.has(url));
    if (added.length === 0) return;
    if (this.files.length > 0) this.onClearFiles();
    this.urlsText = [this.urlsText.trimEnd(), ...added]
      .filter((line) => line.length > 0)
      .join('\n');
  }

  protected focusUrls(): void {
    this.urlsEl?.nativeElement.focus();
  }

  private queueDefaultDir(uuid: string): string {
    return this.queues.find((q) => q.uuid === uuid)?.defaultDirectory ?? '';
  }

  // Follow the queue's default directory, but never overwrite a directory the user typed/picked.
  protected onQueueChange(uuid: string): void {
    const previousDefault = this.queueDefaultDir(this.queueUuid);
    if (this.target === '' || this.target === previousDefault) {
      this.target = this.queueDefaultDir(uuid);
    }
    this.queueUuid = uuid;
  }

  private resetForm(): void {
    this.urlsText = '';
    this.files = [];
    this.queueUuid = this.defaultQueueUuid ?? this.queues[0]?.uuid ?? '';
    this.targetClass = '';
    this.target = this.queueDefaultDir(this.queueUuid);
    this.paused = false;
    this.pickerOpen = false;
    this.speedDown = 0;
    this.speedUp = 0;
    this.error.set('');
    this.saving.set(false);
    this.session++;
    this.queuedUrls = [];
    this.urlsModel?.control.enable();
  }

  protected onFileInputChange(input: HTMLInputElement): void {
    this.addFiles(Array.from(input.files ?? []));
    // Lets the same file be picked again after it was removed from the list.
    input.value = '';
  }

  /** Adds to the chosen files; picking a file that is already listed doesn't list it twice. */
  protected addFiles(files: File[]): void {
    const key = (f: File) => `${f.name}\0${f.size}\0${f.lastModified}`;
    const listed = new Set(this.files.map(key));
    const added: File[] = [];
    for (const f of files) {
      if (listed.has(key(f))) continue;
      listed.add(key(f));
      added.push(f);
    }
    if (added.length > 0) this.setFiles([...this.files, ...added]);
  }

  protected onRemoveFile(file: File): void {
    this.setFiles(this.files.filter((f) => f !== file));
  }

  protected onClearFiles(): void {
    this.setFiles([]);
  }

  private setFiles(files: File[]): void {
    this.files = files;
    if (files.length > 0) this.urlsModel?.control.disable();
    else this.urlsModel?.control.enable();
  }

  /**
   * Uploads the files one request each, in order. Every file is tried even if an earlier one fails;
   * each accepted file leaves the list, so afterwards it holds just the failed ones for a retry.
   */
  private uploadFiles(
    files: File[],
    options: NewTransferOptions,
    session: number,
  ): Observable<void> {
    return from(files).pipe(
      concatMap((file) =>
        this.actions.addTransferWithData(file, options).pipe(
          tap(() => {
            if (session === this.session) this.files = this.files.filter((f) => f !== file);
          }),
          map(() => null),
          catchError((err: unknown) =>
            of(
              `${file.name}: ${err instanceof Error && err.message ? err.message : 'Failed to add the transfer.'}`,
            ),
          ),
        ),
      ),
      toArray(),
      map((results) => {
        const failures = results.filter((r): r is string => r !== null);
        if (failures.length > 0) throw new Error(failures.join('\n'));
      }),
    );
  }

  /** Ctrl-Enter / Cmd-Enter in the URLs textarea or the target input submits the dialog. */
  protected onSubmitShortcut(event: Event): void {
    event.preventDefault();
    this.onSave();
  }

  protected onSave(): void {
    if (this.saving()) return;

    const urls =
      this.files.length > 0
        ? []
        : this.urlsText
            .split('\n')
            .map((line) => line.trim())
            .filter((line) => line.length > 0);
    if (this.files.length === 0 && urls.length === 0) {
      this.error.set('Enter at least one URL or choose a local file.');
      return;
    }

    const options: NewTransferOptions = {
      upload: false,
      queueUuid: this.queueUuid,
      targetClass: this.targetClass,
      target: this.target,
      paused: this.paused,
      speedLimits: { down: (this.speedDown ?? 0) * 1024, up: (this.speedUp ?? 0) * 1024 },
    };
    const session = this.session;
    const request =
      this.files.length > 0
        ? this.uploadFiles(this.files, options, session)
        : this.actions.addTransfers(urls, options);

    this.error.set('');
    this.saving.set(true);
    request.subscribe({
      next: () => {
        if (session !== this.session) return;
        const queued = this.queuedUrls;
        if (queued.length > 0) {
          // Links arrived during the Add: stay open as a fresh form holding just those.
          this.resetForm();
          this.appendUrls(queued);
          this.cdr.markForCheck(); // urlsText is a plain field; the box must re-render
          return;
        }
        this.saving.set(false);
        this.closeDialog();
      },
      error: (err: unknown) => {
        if (session !== this.session) return;
        this.saving.set(false);
        const queued = this.queuedUrls;
        this.queuedUrls = [];
        this.appendUrls(queued);
        this.cdr.markForCheck();
        this.error.set(
          err instanceof Error && err.message ? err.message : 'Failed to add the transfer.',
        );
      },
    });
  }

  protected onCancel(): void {
    this.closeDialog();
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }

  private closeDialog(): void {
    this.onDialogVisibleChange(false);
  }
}
