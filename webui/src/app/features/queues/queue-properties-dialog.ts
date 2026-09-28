import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { InputText } from 'primeng/inputtext';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { ButtonDirective } from 'primeng/button';
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
  imports: [FormsModule, Dialog, InputText, InputNumber, Checkbox, ButtonDirective],
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
    this.speedDown = q?.speedLimits.down ?? 0;
    this.speedUp = q?.speedLimits.up ?? 0;
    this.transferDown = q?.transferLimits.down ?? 0;
    this.transferUp = q?.transferLimits.up ?? 0;
    this.upAsDown = q?.upAsDown ?? false;
  }

  protected get dialogHeader(): string {
    return this.queue === null ? 'New Queue' : 'Queue Properties';
  }

  protected onSave(): void {
    this.save.emit({
      name: this.name,
      defaultDirectory: this.defaultDirectory,
      moveDirectory: this.moveDirectory,
      speedLimits: { down: this.speedDown, up: this.speedUp },
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
