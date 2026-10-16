import { Component, computed, inject, input } from '@angular/core';
import { takeUntilDestroyed, toObservable, toSignal } from '@angular/core/rxjs-interop';
import { catchError, map, of } from 'rxjs';
import { SpeedGraphChart } from './speed-graph-chart';
import { TransferApi } from '../../core/api/transfer-api';
import { TransfersStore } from '../../state/transfers.store';
import { QueuesStore } from '../../state/queues.store';
import { PollingService } from '../../core/polling.service';
import { SpeedSample } from '../../core/api/models';
import { activeGatedPoll } from '../../core/active-gated-poll';

@Component({
  selector: 'app-speed-graphs-tab',
  imports: [SpeedGraphChart],
  templateUrl: './speed-graphs-tab.html',
})
export class SpeedGraphsTab {
  readonly active = input(false);

  private readonly transferApi = inject(TransferApi);
  private readonly transfersStore = inject(TransfersStore);
  protected readonly queuesStore = inject(QueuesStore);
  private readonly polling = inject(PollingService);

  protected readonly selectedTransferUuid = computed(() => {
    const uuids = this.transfersStore.selectedUuids();
    return uuids.length === 1 ? uuids[0] : null;
  });

  protected readonly transferSamples = toSignal(
    activeGatedPoll(
      toObservable(this.active),
      toObservable(this.selectedTransferUuid),
      this.polling.ticks$,
      (uuid: string | null) =>
        uuid
          ? this.transferApi.getTransferSpeedGraph(uuid).pipe(
              catchError((err) => {
                console.error('Failed to fetch transfer speed graph', err);
                return of<SpeedSample[]>([]);
              }),
            )
          : of<SpeedSample[]>([]),
    ).pipe(
      map((emission) => emission.value),
      takeUntilDestroyed(),
    ),
    { initialValue: [] as SpeedSample[] },
  );

  protected readonly queueSamples = toSignal(
    activeGatedPoll(
      toObservable(this.active),
      toObservable(this.queuesStore.selectedQueueUuid),
      this.polling.ticks$,
      (uuid: string | null) =>
        uuid
          ? this.transferApi.getQueueSpeedGraph(uuid).pipe(
              catchError((err) => {
                console.error('Failed to fetch queue speed graph', err);
                return of<SpeedSample[]>([]);
              }),
            )
          : of<SpeedSample[]>([]),
    ).pipe(
      map((emission) => emission.value),
      takeUntilDestroyed(),
    ),
    { initialValue: [] as SpeedSample[] },
  );
}
