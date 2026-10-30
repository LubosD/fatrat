import {
  ChangeDetectorRef,
  Component,
  computed,
  inject,
  signal,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ConfirmationService } from 'primeng/api';
import { TableModule } from 'primeng/table';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { ButtonDirective } from 'primeng/button';
import { TransferApi } from '../../core/api/transfer-api';
import { RssApi } from '../../core/rss-api.service';
import { ApiError } from '../../core/api/rest-client';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { QueuesStore } from '../../state/queues.store';
import { ViewportService } from '../../core/viewport.service';
import { JsonValue } from '../../core/api/json-value';
import { RssFeed, RssRegexp, RssTvsMode } from '../../core/api/models';
import { RssFeedDialog, RssFeedFormValue } from './rss-feed-dialog';
import { RssRegexpDialog, RssRegexpFormValue } from './rss-regexp-dialog';
import { RssDownloadedDialog } from './rss-downloaded-dialog';

const KEYS = ['rss/enable', 'rss/interval'] as const;

const TVS_LABELS: Record<RssTvsMode, string> = {
  none: '',
  season: 'Season-based',
  episode: 'Episode-based',
  date: 'Date-based',
};

/**
 * The enable switch and the interval are staged in the settings dialog like every other page and
 * saved with OK. Feeds and rules are saved as soon as they are changed: the server also rewrites
 * the rules in the background (the list of downloaded episodes), so a list saved later from a
 * stale copy would overwrite that.
 */
@Component({
  selector: 'app-rss-settings',
  imports: [
    FormsModule,
    TableModule,
    InputNumber,
    Checkbox,
    ButtonDirective,
    RssFeedDialog,
    RssRegexpDialog,
    RssDownloadedDialog,
  ],
  templateUrl: './rss-settings.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './rss-dialogs.scss',
})
export class RssSettings {
  private readonly transferApi = inject(TransferApi);
  private readonly api = inject(RssApi);
  private readonly draft = inject(SettingsDraftService);
  private readonly cdr = inject(ChangeDetectorRef);
  private readonly confirmation = inject(ConfirmationService);
  private readonly queuesStore = inject(QueuesStore);
  protected readonly viewport = inject(ViewportService);

  protected enable = false;
  protected interval = 15;

  protected readonly feeds = signal<RssFeed[]>([]);
  protected readonly regexps = signal<RssRegexp[]>([]);
  protected readonly queues = this.queuesStore.queues;
  protected readonly message = signal('');

  protected readonly feedDialogOpen = signal(false);
  protected readonly editedFeed = signal<RssFeed | null>(null);
  protected readonly feedError = signal('');

  protected readonly regexpDialogOpen = signal(false);
  protected readonly editedRegexp = signal<RssRegexp | null>(null);
  protected readonly regexpError = signal('');

  protected readonly downloadedDialogOpen = signal(false);
  protected readonly downloadedRegexp = signal<RssRegexp | null>(null);

  protected readonly canAddRegexp = computed(
    () => this.feeds().length > 0 && this.queues().length > 0,
  );

  constructor() {
    this.transferApi.getSettingsValues([...KEYS]).subscribe({
      next: (values) => {
        const raw = new Map<string, JsonValue>(KEYS.map((key, i) => [key, values[i]]));
        const get = (key: string): JsonValue | undefined => this.draft.get(key) ?? raw.get(key);
        this.enable = Boolean(get('rss/enable') ?? false);
        this.interval = Number(get('rss/interval') ?? 15);
        // The app is zoneless and these fields are plain properties, so tell Angular to re-render.
        this.cdr.markForCheck();
      },
      error: (err) => console.error('Failed to load RSS settings', err),
    });
    this.reload();
  }

  protected stage(key: string, value: JsonValue): void {
    this.draft.stage(key, value);
  }

  protected feedName(feedId: string): string {
    return this.feeds().find((f) => f.id === feedId)?.name ?? '';
  }

  protected queueName(uuid: string): string {
    return this.queues().find((q) => q.uuid === uuid)?.name ?? '';
  }

  protected tvsLabel(tvs: RssTvsMode): string {
    return TVS_LABELS[tvs];
  }

  /** Rules are listed as "expression @ feed", like the desktop UI. */
  protected regexpLabel(r: RssRegexp): string {
    const feed = this.feedName(r.feedId);
    return r.expression === '' ? feed : `${r.expression} @ ${feed}`;
  }

  /** The server may have dropped rules (their queue was deleted) since the last look. */
  protected reload(): void {
    this.api.listFeeds().subscribe({
      next: (feeds) => this.feeds.set(feeds),
      error: (err: ApiError) => this.message.set(err.message),
    });
    this.api.listRegexps().subscribe({
      next: (regexps) => this.regexps.set(regexps),
      error: (err: ApiError) => this.message.set(err.message),
    });
  }

  protected updateNow(): void {
    this.api.refresh().subscribe({
      next: () => this.message.set('Updating the feeds…'),
      error: (err: ApiError) => this.message.set(err.message),
    });
  }

  // ---- feeds ----

  protected addFeed(): void {
    this.editedFeed.set(null);
    this.feedError.set('');
    this.feedDialogOpen.set(true);
  }

  protected editFeed(feed: RssFeed): void {
    this.editedFeed.set(feed);
    this.feedError.set('');
    this.feedDialogOpen.set(true);
  }

  protected saveFeed(value: RssFeedFormValue): void {
    const edited = this.editedFeed();
    const request =
      edited === null ? this.api.createFeed(value) : this.api.updateFeed(edited.id, value);
    request.subscribe({
      next: () => {
        this.feedDialogOpen.set(false);
        this.reload();
      },
      error: (err: ApiError) => this.feedError.set(err.message),
    });
  }

  protected deleteFeed(feed: RssFeed): void {
    const count = this.regexps().filter((r) => r.feedId === feed.id).length;
    this.confirmation.confirm({
      header: 'Confirm Delete',
      message:
        count === 0
          ? `Delete the feed "${feed.name}"?`
          : `Delete the feed "${feed.name}" and its ${count} ${count === 1 ? 'rule' : 'rules'}?`,
      accept: () =>
        this.api.deleteFeed(feed.id).subscribe({
          next: () => this.reload(),
          error: (err: ApiError) => this.message.set(err.message),
        }),
    });
  }

  // ---- regexps ----

  protected addRegexp(): void {
    if (!this.canAddRegexp()) return;
    this.editedRegexp.set(null);
    this.regexpError.set('');
    this.regexpDialogOpen.set(true);
  }

  protected editRegexp(regexp: RssRegexp): void {
    this.editedRegexp.set(regexp);
    this.regexpError.set('');
    this.regexpDialogOpen.set(true);
  }

  protected saveRegexp(value: RssRegexpFormValue): void {
    const edited = this.editedRegexp();
    const request =
      edited === null ? this.api.createRegexp(value) : this.api.updateRegexp(edited.id, value);
    request.subscribe({
      next: () => {
        this.regexpDialogOpen.set(false);
        this.reload();
      },
      error: (err: ApiError) => this.regexpError.set(err.message),
    });
  }

  protected deleteRegexp(regexp: RssRegexp): void {
    this.confirmation.confirm({
      header: 'Confirm Delete',
      message: `Delete the rule "${this.regexpLabel(regexp)}"?`,
      accept: () =>
        this.api.deleteRegexp(regexp.id).subscribe({
          next: () => this.reload(),
          error: (err: ApiError) => this.message.set(err.message),
        }),
    });
  }

  protected manageDownloaded(regexp: RssRegexp): void {
    this.downloadedRegexp.set(regexp);
    this.downloadedDialogOpen.set(true);
  }
}
