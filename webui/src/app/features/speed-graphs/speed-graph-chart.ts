import { Component, Input, OnChanges, SimpleChanges, ChangeDetectionStrategy } from '@angular/core';
import { UIChart } from '@openng/optimus-ui/chart';
import type { ChartData, ChartOptions } from 'chart.js';
import { SpeedSample } from '../../core/api/models';
import { formatSpeed } from '../transfers/format';

@Component({
  selector: 'app-speed-graph-chart',
  imports: [UIChart],
  changeDetection: ChangeDetectionStrategy.Eager,
  templateUrl: './speed-graph-chart.html',
})
export class SpeedGraphChart implements OnChanges {
  @Input() samples: SpeedSample[] = [];
  @Input({ required: true }) title!: string;

  protected chartData: ChartData<'line'> = { labels: [], datasets: [] };

  protected readonly chartOptions: ChartOptions<'line'> = {
    responsive: true,
    animation: false,
    scales: {
      x: { display: false },
      y: {
        beginAtZero: true,
        ticks: { callback: (value) => formatSpeed(Number(value)) },
      },
    },
    plugins: {
      tooltip: {
        callbacks: {
          label: (ctx) => `${ctx.dataset.label}: ${formatSpeed(Number(ctx.parsed.y))}`,
        },
      },
    },
  };

  protected get hasData(): boolean {
    return this.samples.length > 0;
  }

  ngOnChanges(changes: SimpleChanges): void {
    if ('samples' in changes) {
      this.chartData = {
        labels: this.samples.map((_, i) => i),
        datasets: [
          {
            label: 'Download',
            data: this.samples.map((s) => s.down),
            borderColor: '#3b82f6',
            fill: false,
            pointRadius: 0,
          },
          {
            label: 'Upload',
            data: this.samples.map((s) => s.up),
            borderColor: '#f97316',
            fill: false,
            pointRadius: 0,
          },
        ],
      };
    }
  }
}
