import { Component, inject, signal } from '@angular/core';
import { TransferApi } from './core/api/transfer-api';
import { Queue } from './core/api/models';

@Component({
  selector: 'app-root',
  templateUrl: './app.html',
  styleUrl: './app.scss',
})
export class App {
  private readonly transferApi = inject(TransferApi);
  readonly queues = signal<Queue[] | null>(null);
  readonly error = signal<string | null>(null);

  constructor() {
    this.transferApi.getQueues().subscribe({
      next: (queues) => this.queues.set(queues),
      error: (err) => this.error.set(String(err)),
    });
  }
}
