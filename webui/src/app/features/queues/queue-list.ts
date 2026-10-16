import { Component, inject } from '@angular/core';
import { QueuesStore } from '../../state/queues.store';

@Component({
  selector: 'app-queue-list',
  templateUrl: './queue-list.html',
  styleUrl: './queue-list.scss',
})
export class QueueList {
  protected readonly store = inject(QueuesStore);
}
