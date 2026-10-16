import { Injectable, computed, signal } from '@angular/core';
import { Observable, tap } from 'rxjs';

/**
 * Tracks whether the periodic refreshes (queues, transfers, stats) are succeeding, so the UI can
 * tell the user that what it shows is stale — e.g. because connectivity to the server was lost.
 */
@Injectable({ providedIn: 'root' })
export class ConnectionHealth {
  private readonly failing = signal<ReadonlySet<string>>(new Set());

  /** True while at least one polled source's latest refresh failed. */
  readonly refreshFailing = computed(() => this.failing().size > 0);

  /** Records the outcome of every request of `source`; errors are passed through unchanged. */
  track<T>(source: string, request$: Observable<T>): Observable<T> {
    return request$.pipe(
      tap({
        next: () => this.set(source, false),
        error: () => this.set(source, true),
      }),
    );
  }

  private set(source: string, failed: boolean): void {
    const current = this.failing();
    if (current.has(source) === failed) return;
    const next = new Set(current);
    if (failed) next.add(source);
    else next.delete(source);
    this.failing.set(next);
  }
}
