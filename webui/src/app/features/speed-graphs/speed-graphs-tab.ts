import {
  Component,
  Input,
  OnChanges,
  OnDestroy,
  SimpleChanges,
  computed,
  effect,
  inject,
  signal,
} from '@angular/core';
import { Subscription } from 'rxjs';
import { SpeedGraphChart } from './speed-graph-chart';
import { TransferApi } from '../../core/api/transfer-api';
import { TransfersStore } from '../../state/transfers.store';
import { QueuesStore } from '../../state/queues.store';
import { PollingService } from '../../core/polling.service';
import { SpeedSample } from '../../core/api/models';

@Component({
  selector: 'app-speed-graphs-tab',
  imports: [SpeedGraphChart],
  templateUrl: './speed-graphs-tab.html',
})
export class SpeedGraphsTab implements OnChanges, OnDestroy {
  @Input() active = false;

  private readonly transferApi = inject(TransferApi);
  private readonly transfersStore = inject(TransfersStore);
  protected readonly queuesStore = inject(QueuesStore);
  private readonly polling = inject(PollingService);

  protected readonly selectedTransferUuid = computed(() => {
    const uuids = this.transfersStore.selectedUuids();
    return uuids.length === 1 ? uuids[0] : null;
  });

  protected readonly transferSamples = signal<SpeedSample[]>([]);
  protected readonly queueSamples = signal<SpeedSample[]>([]);

  private pollSubscription: Subscription | null = null;

  /**
   * The transfer/queue uuids we last saw the selection effect fire for. Plain instance fields
   * rather than signals, mirroring TransferDetails.lastFetchedUuid. Seeded (via field
   * initializers, which run before the constructor body below) to the *current* signal values
   * rather than a sentinel: ngOnChanges already issues the activation refresh() when `active`
   * transitions to true, and this effect's first run happens on the same change-detection pass.
   * Seeding to the current values means that first run sees "no change" and stays a no-op, so we
   * don't double-fetch at the exact moment of activation; any real selection change afterwards
   * still differs from these seeded values and is caught normally.
   */
  private lastTransferUuid: string | null | undefined = this.selectedTransferUuid();
  private lastQueueUuid: string | null | undefined = this.queuesStore.selectedQueueUuid();

  constructor() {
    effect(() => {
      const transferUuid = this.selectedTransferUuid();
      const queueUuid = this.queuesStore.selectedQueueUuid();
      const selectionChanged =
        transferUuid !== this.lastTransferUuid || queueUuid !== this.lastQueueUuid;
      this.lastTransferUuid = transferUuid;
      this.lastQueueUuid = queueUuid;
      if (selectionChanged && this.active) {
        this.refresh();
      }
    });
  }

  ngOnChanges(changes: SimpleChanges): void {
    if (!('active' in changes)) {
      return;
    }
    if (this.active) {
      this.refresh();
      this.pollSubscription = this.polling.ticks$.subscribe(() => this.refresh());
    } else {
      this.pollSubscription?.unsubscribe();
      this.pollSubscription = null;
    }
  }

  ngOnDestroy(): void {
    this.pollSubscription?.unsubscribe();
  }

  private refresh(): void {
    const transferUuid = this.selectedTransferUuid();
    if (transferUuid) {
      this.transferApi.getTransferSpeedGraph(transferUuid).subscribe({
        next: (samples) => {
          if (this.selectedTransferUuid() === transferUuid) {
            this.transferSamples.set(samples);
          }
        },
        error: (err) => console.error('Failed to fetch transfer speed graph', err),
      });
    } else {
      this.transferSamples.set([]);
    }

    const queueUuid = this.queuesStore.selectedQueueUuid();
    if (queueUuid) {
      this.transferApi.getQueueSpeedGraph(queueUuid).subscribe({
        next: (samples) => {
          if (this.queuesStore.selectedQueueUuid() === queueUuid) {
            this.queueSamples.set(samples);
          }
        },
        error: (err) => console.error('Failed to fetch queue speed graph', err),
      });
    } else {
      this.queueSamples.set([]);
    }
  }
}
