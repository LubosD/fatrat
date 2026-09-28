import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { Textarea } from 'primeng/textarea';
import { InputNumber } from 'primeng/inputnumber';
import { ButtonDirective } from 'primeng/button';
import { Transfer } from '../../core/api/models';

export interface TransferPropertiesFormValue {
  comment: string;
  speedLimits: { down: number; up: number };
}

@Component({
  selector: 'app-transfer-properties-dialog',
  imports: [FormsModule, Dialog, Textarea, InputNumber, ButtonDirective],
  templateUrl: './transfer-properties-dialog.html',
})
export class TransferPropertiesDialog implements OnChanges {
  @Input() transfer: Transfer | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<TransferPropertiesFormValue>();

  comment = '';
  speedDown = 0;
  speedUp = 0;

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if ('transfer' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
    }
  }

  private resetForm(): void {
    const t = this.transfer;
    this.comment = t?.comment ?? '';
    this.speedDown = t?.userSpeedLimits.down ?? 0;
    this.speedUp = t?.userSpeedLimits.up ?? 0;
  }

  protected onSave(): void {
    this.save.emit({ comment: this.comment, speedLimits: { down: this.speedDown, up: this.speedUp } });
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
