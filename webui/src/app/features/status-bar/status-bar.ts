import { Component, inject } from '@angular/core';
import { ConnectionHealth } from '../../core/connection-health.service';
import { StatsStore } from '../../state/stats.store';
import { formatSpeed } from '../transfers/format';
import { TasksIndicator } from '../tasks/tasks-indicator';

@Component({
  selector: 'app-status-bar',
  imports: [TasksIndicator],
  templateUrl: './status-bar.html',
  styleUrl: './status-bar.scss',
})
export class StatusBar {
  protected readonly stats = inject(StatsStore);
  protected readonly health = inject(ConnectionHealth);

  protected readonly formatSpeed = formatSpeed;
}
