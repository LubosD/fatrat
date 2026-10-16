import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { InputText } from 'primeng/inputtext';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { ButtonDirective } from 'primeng/button';
import { InputGroup } from 'primeng/inputgroup';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { Queue } from '../../core/api/models';

export interface QueueFormValue {
  name: string;
  defaultDirectory: string;
  moveDirectory: string;
  speedLimits: { down: number; up: number };
  transferLimits: { down: number; up: number };
  upAsDown: boolean;
}

@Component({
  selector: 'app-queue-properties-dialog',
  imports: [FormsModule, Dialog, InputText, InputNumber, Checkbox, ButtonDirective, InputGroup, DirectoryPicker],
  templateUrl: './queue-properties-dialog.html',
})
export class QueuePropertiesDialog implements OnChanges {
  @Input() queue: Queue | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<QueueFormValue>();

  name = '';
  defaultDirectory = '';
  moveDirectory = '';
  speedDown = 0;
  speedUp = 0;
  transferDown = 0;
  transferUp = 0;
  upAsDown = false;
  protected pickerTarget: 'defaultDirectory' | 'moveDirectory' | null = null;

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if ('queue' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
    }
  }

  private resetForm(): void {
    const q = this.queue;
    this.name = q?.name ?? '';
    this.defaultDirectory = q?.defaultDirectory ?? '';
    this.moveDirectory = q?.moveDirectory ?? '';
    this.speedDown = Math.round((q?.speedLimits.down ?? 0) / 1024);
    this.speedUp = Math.round((q?.speedLimits.up ?? 0) / 1024);
    this.transferDown = q?.transferLimits.down ?? 0;
    this.transferUp = q?.transferLimits.up ?? 0;
    this.upAsDown = q?.upAsDown ?? false;
    this.pickerTarget = null;
  }

  protected onDirectoryPicked(path: string): void {
    if (this.pickerTarget !== null) this[this.pickerTarget] = path;
  }

  protected get dialogHeader(): string {
    return this.queue === null ? 'New Queue' : 'Queue Properties';
  }

  protected onSave(): void {
    this.save.emit({
      name: this.name,
      defaultDirectory: this.defaultDirectory,
      moveDirectory: this.moveDirectory,
      speedLimits: { down: (this.speedDown ?? 0) * 1024, up: (this.speedUp ?? 0) * 1024 },
      transferLimits: { down: this.transferDown, up: this.transferUp },
      upAsDown: this.upAsDown,
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
