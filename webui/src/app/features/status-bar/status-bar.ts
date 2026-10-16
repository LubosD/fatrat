import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ButtonDirective } from 'primeng/button';
import { InputNumber } from 'primeng/inputnumber';
import { Popover } from 'primeng/popover';
import { TransferApi } from '../../core/api/transfer-api';
import { StatsStore } from '../../state/stats.store';
import { formatSpeed } from '../transfers/format';

type Direction = 'down' | 'up';

const SETTING_KEYS = ['network/speed_down', 'network/speed_up'];
const PRESETS_KB = [50, 100, 250, 500, 1000, 2500];

@Component({
  selector: 'app-status-bar',
  imports: [FormsModule, ButtonDirective, InputNumber, Popover],
  templateUrl: './status-bar.html',
  styleUrl: './status-bar.scss',
})
export class StatusBar {
  private readonly api = inject(TransferApi);
  protected readonly stats = inject(StatsStore);

  protected readonly formatSpeed = formatSpeed;
  protected readonly presets = PRESETS_KB;

  /** Global limits in bytes/s; 0 means unlimited. */
  protected readonly limits = signal<{ down: number; up: number }>({ down: 0, up: 0 });
  protected readonly editing = signal<Direction>('down');
  protected limitKb = 0;

  constructor() {
    this.loadLimits();
  }

  protected formatLimit(bytesPerSecond: number): string {
    return bytesPerSecond > 0 ? formatSpeed(bytesPerSecond) : '∞';
  }

  protected open(popover: Popover, event: Event, direction: Direction): void {
    this.editing.set(direction);
    this.limitKb = Math.round(this.limits()[direction] / 1024);
    popover.toggle(event);
  }

  protected apply(popover: Popover, kb: number | null): void {
    const bytes = Math.max(0, Math.round(kb ?? 0)) * 1024;
    const next = { ...this.limits(), [this.editing()]: bytes };
    this.api.setSettingsValues(SETTING_KEYS, [next.down, next.up]).subscribe(() => {
      this.limits.set(next);
      this.api.applySettings().subscribe();
    });
    popover.hide();
  }

  private loadLimits(): void {
    this.api.getSettingsValues(SETTING_KEYS).subscribe(([down, up]) => {
      this.limits.set({ down: Number(down) || 0, up: Number(up) || 0 });
    });
  }
}
