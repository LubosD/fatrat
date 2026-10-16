import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges, ViewChild } from '@angular/core';
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
import { Queue, TransferClass } from '../../core/api/models';

export interface NewTransferFormValue {
  urls: string[];
  file: File | null;
  queueUuid: string;
  targetClass: string;
  target: string;
  paused: boolean;
  speedLimits: { down: number; up: number };
}

@Component({
  selector: 'app-new-transfer-dialog',
  imports: [FormsModule, Dialog, Textarea, InputText, InputNumber, Checkbox, Select, FileUpload, ButtonDirective, InputGroup, DirectoryPicker],
  templateUrl: './new-transfer-dialog.html',
})
export class NewTransferDialog implements OnChanges {
  @Input() queues: Queue[] = [];
  @Input() transferClasses: TransferClass[] = [];
  @Input() defaultQueueUuid: string | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<NewTransferFormValue>();

  @ViewChild('urlsModel') private urlsModel?: NgModel;

  urlsText = '';
  file: File | null = null;
  queueUuid = '';
  targetClass = '';
  target = '';
  paused = false;
  protected pickerOpen = false;
  speedDown = 0;
  speedUp = 0;

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
    }
  }

  private resetForm(): void {
    this.urlsText = '';
    this.file = null;
    this.queueUuid = this.defaultQueueUuid ?? this.queues[0]?.uuid ?? '';
    this.targetClass = '';
    this.target = '';
    this.paused = false;
    this.pickerOpen = false;
    this.speedDown = 0;
    this.speedUp = 0;
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

  protected onSave(): void {
    const urls =
      this.file !== null
        ? []
        : this.urlsText
            .split('\n')
            .map((line) => line.trim())
            .filter((line) => line.length > 0);

    this.save.emit({
      urls,
      file: this.file,
      queueUuid: this.queueUuid,
      targetClass: this.targetClass,
      target: this.target,
      paused: this.paused,
      speedLimits: { down: (this.speedDown ?? 0) * 1024, up: (this.speedUp ?? 0) * 1024 },
    });
    this.closeDialog();
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
