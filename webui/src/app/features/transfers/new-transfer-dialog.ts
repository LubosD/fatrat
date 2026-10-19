import { Component, signal, ElementRef, EventEmitter, inject, Input, OnChanges, Output, SimpleChanges, ViewChild } from '@angular/core';
import { FormsModule, NgModel } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { Textarea } from 'primeng/textarea';
import { InputText } from 'primeng/inputtext';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { Select } from 'primeng/select';
import { FileUpload, FileSelectEvent } from 'primeng/fileupload';
import { ButtonDirective } from 'primeng/button';
import { InputGroup } from 'primeng/inputgroup';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { NewTransferOptions, Queue, TransferClass } from '../../core/api/models';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { PathSuggest } from '../../shared/path-suggest/path-suggest';

@Component({
  selector: 'app-new-transfer-dialog',
  imports: [FormsModule, Dialog, Textarea, InputText, InputNumber, Checkbox, Select, FileUpload, ButtonDirective, InputGroup, DirectoryPicker, PathSuggest],
  templateUrl: './new-transfer-dialog.html',
  styleUrl: './new-transfer-dialog.scss',
})
/**
 * Adds the transfers itself: on a failure it stays open with the server's message and the typed
 * input, and it closes only once the server accepted everything.
 */
export class NewTransferDialog implements OnChanges {
  private readonly actions = inject(TransferActionsService);
  @Input() queues: Queue[] = [];
  @Input() transferClasses: TransferClass[] = [];
  @Input() defaultQueueUuid: string | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();

  @ViewChild('urlsModel') private urlsModel?: NgModel;
  @ViewChild('urlsModel', { read: ElementRef }) private urlsEl?: ElementRef<HTMLTextAreaElement>;

  urlsText = '';
  file: File | null = null;
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

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if (changes['visible']?.currentValue === true || (!this.visible && ('defaultQueueUuid' in changes || 'queues' in changes))) {
      this.resetForm();
    } else if (this.visible && !this.queueUuid) {
      // Background polling hands us a fresh `queues` array every refresh; while the dialog is open
      // that must not reset the form or close the directory picker — only fill in a missing queue.
      this.queueUuid = this.defaultQueueUuid ?? this.queues[0]?.uuid ?? '';
      this.target = this.queueDefaultDir(this.queueUuid);
    }
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
    this.file = null;
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
    this.urlsModel?.control.enable();
  }

  protected onFileSelect(event: FileSelectEvent): void {
    this.file = event.files[0] ?? null;
    this.urlsModel?.control.disable();
  }

  protected onClearFile(): void {
    this.file = null;
    this.urlsModel?.control.enable();
  }

  /** Ctrl-Enter / Cmd-Enter in the URLs textarea or the target input submits the dialog. */
  protected onSubmitShortcut(event: Event): void {
    event.preventDefault();
    this.onSave();
  }

  protected onSave(): void {
    if (this.saving()) return;

    const urls =
      this.file !== null
        ? []
        : this.urlsText
            .split('\n')
            .map((line) => line.trim())
            .filter((line) => line.length > 0);
    if (this.file === null && urls.length === 0) {
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
    const request = this.file !== null ? this.actions.addTransferWithData(this.file, options) : this.actions.addTransfers(urls, options);

    const session = this.session;
    this.error.set('');
    this.saving.set(true);
    request.subscribe({
      next: () => {
        if (session !== this.session) return;
        this.saving.set(false);
        this.closeDialog();
      },
      error: (err: unknown) => {
        if (session !== this.session) return;
        this.saving.set(false);
        this.error.set(err instanceof Error && err.message ? err.message : 'Failed to add the transfer.');
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
