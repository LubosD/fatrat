import { Component, inject } from '@angular/core';
import { TableModule } from 'primeng/table';
import { ProgressBar } from 'primeng/progressbar';
import { TransfersStore } from '../../state/transfers.store';
import { formatEta, formatSize, formatSpeed } from './format';

@Component({
  selector: 'app-transfer-table',
  imports: [TableModule, ProgressBar],
  templateUrl: './transfer-table.html',
})
export class TransferTable {
  protected readonly store = inject(TransfersStore);
  protected readonly formatSize = formatSize;
  protected readonly formatSpeed = formatSpeed;

  protected progress(total: number, done: number): number {
    return total > 0 ? Math.round((done / total) * 100) : 0;
  }

  protected eta(total: number, done: number, downSpeed: number): string {
    return formatEta(total, done, downSpeed);
  }
}
