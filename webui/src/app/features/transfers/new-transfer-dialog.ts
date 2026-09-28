import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { Textarea } from 'primeng/textarea';
import { InputText } from 'primeng/inputtext';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { Select } from 'primeng/select';
import { FileUpload, FileSelectEvent } from 'primeng/fileupload';
import { ButtonDirective } from 'primeng/button';
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
  imports: [FormsModule, Dialog, Textarea, InputText, InputNumber, Checkbox, Select, FileUpload, ButtonDirective],
  templateUrl: './new-transfer-dialog.html',
})
export class NewTransferDialog implements OnChanges {
  @Input() queues: Queue[] = [];
  @Input() transferClasses: TransferClass[] = [];
  @Input() defaultQueueUuid: string | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<NewTransferFormValue>();

  urlsText = '';
  file: File | null = null;
  queueUuid = '';
  targetClass = '';
  target = '';
  paused = false;
  speedDown = 0;
  speedUp = 0;

  ngOnChanges(changes: SimpleChanges): void {
    if ('defaultQueueUuid' in changes || 'queues' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
    }
  }

  private resetForm(): void {
    this.urlsText = '';
    this.file = null;
    this.queueUuid = this.defaultQueueUuid ?? this.queues[0]?.uuid ?? '';
    this.targetClass = '';
    this.target = '';
    this.paused = false;
    this.speedDown = 0;
    this.speedUp = 0;
  }

  protected onFileSelect(event: FileSelectEvent): void {
    this.file = event.files[0] ?? null;
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
      speedLimits: { down: this.speedDown, up: this.speedUp },
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
