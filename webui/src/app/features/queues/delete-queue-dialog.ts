import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { RadioButton } from 'primeng/radiobutton';
import { Select } from 'primeng/select';
import { ButtonDirective } from 'primeng/button';
import { DeleteQueueOptions, Queue } from '../../core/api/models';

type DeleteMode = 'move' | 'delete' | 'deleteWithData';

@Component({
  selector: 'app-delete-queue-dialog',
  imports: [FormsModule, Dialog, RadioButton, Select, ButtonDirective],
  templateUrl: './delete-queue-dialog.html',
})
export class DeleteQueueDialog implements OnChanges {
  @Input() queue: Queue | null = null;
  @Input() otherQueues: Queue[] = [];
  @Input() transferCount = 0;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() confirmed = new EventEmitter<DeleteQueueOptions>();

  mode: DeleteMode = 'delete';
  moveTo = '';

  ngOnChanges(changes: SimpleChanges): void {
    if ('queue' in changes || changes['visible']?.currentValue === true) {
      this.moveTo = this.otherQueues[0]?.uuid ?? '';
      this.mode = this.otherQueues.length > 0 ? 'move' : 'delete';
    }
  }

  protected onDelete(): void {
    this.confirmed.emit(
      this.mode === 'move' ? { moveTo: this.moveTo } : { deleteTransfers: true, withData: this.mode === 'deleteWithData' },
    );
    this.close();
  }

  protected close(): void {
    this.visibleChange.emit(false);
  }
}
