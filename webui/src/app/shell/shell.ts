import { Component, inject } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Toolbar } from 'primeng/toolbar';
import { Select } from 'primeng/select';
import { Splitter } from 'primeng/splitter';
import { QueueList } from '../features/queues/queue-list';
import { TransferTable } from '../features/transfers/transfer-table';
import { PollingService } from '../core/polling.service';

@Component({
  selector: 'app-shell',
  imports: [FormsModule, Toolbar, Select, Splitter, QueueList, TransferTable],
  templateUrl: './shell.html',
  styleUrl: './shell.scss',
})
export class Shell {
  protected readonly polling = inject(PollingService);
  protected readonly allowedIntervals: number[] = [...this.polling.allowedIntervals];
}
