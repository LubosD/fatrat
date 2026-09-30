import { Component, ElementRef, Input, OnChanges, OnDestroy, SimpleChanges, ViewChild, computed, inject, signal } from '@angular/core';
import { Subscription } from 'rxjs';
import { LogService } from '../../core/log.service';
import { TransfersStore } from '../../state/transfers.store';
import { PollingService } from '../../core/polling.service';

@Component({
  selector: 'app-log-tab',
  templateUrl: './log-tab.html',
})
export class LogTab implements OnChanges, OnDestroy {
  @Input() active = false;

  private readonly logService = inject(LogService);
  private readonly transfersStore = inject(TransfersStore);
  private readonly polling = inject(PollingService);

  protected readonly globalLog = signal('');
  protected readonly transferLog = signal('');

  protected readonly selectedTransferUuid = computed(() => {
    const uuids = this.transfersStore.selectedUuids();
    return uuids.length === 1 ? uuids[0] : null;
  });

  @ViewChild('globalLogEl') private globalLogEl?: ElementRef<HTMLElement>;
  @ViewChild('transferLogEl') private transferLogEl?: ElementRef<HTMLElement>;

  private pollSubscription: Subscription | null = null;

  ngOnChanges(changes: SimpleChanges): void {
    if (!('active' in changes)) {
      return;
    }
    if (this.active) {
      this.refresh(true);
      this.pollSubscription = this.polling.ticks$.subscribe(() => this.refresh(false));
    } else {
      this.pollSubscription?.unsubscribe();
      this.pollSubscription = null;
    }
  }

  ngOnDestroy(): void {
    this.pollSubscription?.unsubscribe();
  }

  private refresh(scrollOnLoad: boolean): void {
    this.logService.getGlobalLog().subscribe({
      next: (text) => {
        this.globalLog.set(text);
        if (scrollOnLoad) {
          this.scrollToBottom();
        }
      },
      error: (err) => console.error('Failed to fetch global log', err),
    });

    const transferUuid = this.selectedTransferUuid();
    if (transferUuid) {
      this.logService.getTransferLog(transferUuid).subscribe({
        next: (text) => {
          if (this.selectedTransferUuid() === transferUuid) {
            this.transferLog.set(text);
            if (scrollOnLoad) {
              this.scrollToBottom();
            }
          }
        },
        error: (err) => console.error('Failed to fetch transfer log', err),
      });
    } else {
      this.transferLog.set('');
    }
  }

  /**
   * Extracted so tests can spy on it instead of asserting real scrollTop/scrollHeight values
   * (fragile under this project's happy-dom test environment, which has no real layout engine —
   * same rationale as TransferDetails's navigateTo() extraction in Phase 3a). Scrolls whichever
   * of the two panels currently exist in the DOM (the transfer-log panel may be absent when no
   * single transfer is selected).
   */
  private scrollToBottom(): void {
    for (const ref of [this.globalLogEl, this.transferLogEl]) {
      if (ref) {
        ref.nativeElement.scrollTop = ref.nativeElement.scrollHeight;
      }
    }
  }
}
