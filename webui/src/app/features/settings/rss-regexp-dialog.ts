import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  OnDestroy,
  Output,
  SimpleChanges,
  signal,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { InputText } from 'primeng/inputtext';
import { Checkbox } from 'primeng/checkbox';
import { RadioButton } from 'primeng/radiobutton';
import { Select } from 'primeng/select';
import { Tabs, TabList, Tab, TabPanels, TabPanel } from 'primeng/tabs';
import { ButtonDirective } from 'primeng/button';
import { InputGroup } from 'primeng/inputgroup';
import { Subject, Subscription, of } from 'rxjs';
import { catchError, debounceTime, switchMap } from 'rxjs/operators';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { RssApi } from '../../core/rss-api.service';
import { Queue, RssFeed, RssRegexp, RssTestResult, RssTvsMode } from '../../core/api/models';
import { EPISODE_EXAMPLE, RssEpisodeMode, defaultRange, isValidEpisode } from './rss-episode';
import { PathSuggest } from '../../shared/path-suggest/path-suggest';

export interface RssRegexpFormValue {
  feedId: string;
  queue: string;
  expression: string;
  target: string;
  addPaused: boolean;
  tvs: RssTvsMode;
  from: string;
  to: string;
  includeRepacks: boolean;
  includeTrailers: boolean;
  excludeManuals: boolean;
  linkRegexp: string;
}

type TestState = { kind: 'idle' } | { kind: 'result'; result: RssTestResult } | { kind: 'invalid' };

/**
 * Does not close itself on Save: the parent performs the request and either closes it or sets
 * `error`, so the user keeps what they typed.
 */
@Component({
  selector: 'app-rss-regexp-dialog',
  imports: [
    FormsModule,
    Dialog,
    InputText,
    Checkbox,
    RadioButton,
    Select,
    ButtonDirective,
    InputGroup,
    Tabs,
    TabList,
    Tab,
    TabPanels,
    TabPanel,
    DirectoryPicker,
    PathSuggest,
  ],
  templateUrl: './rss-regexp-dialog.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './rss-dialogs.scss',
})
export class RssRegexpDialog implements OnChanges, OnDestroy {
  /** null: a new rule. */
  @Input() regexp: RssRegexp | null = null;
  @Input() feeds: RssFeed[] = [];
  @Input() queues: Queue[] = [];
  @Input() visible = false;
  @Input() error = '';
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<RssRegexpFormValue>();
  @Output() manageDownloaded = new EventEmitter<RssRegexp>();

  feedId = '';
  queue = '';
  expression = '';
  target = '';
  addPaused = false;
  tvs: RssTvsMode = 'none';
  from = '';
  to = '';
  includeRepacks = true;
  includeTrailers = false;
  excludeManuals = true;
  extractLinks = false;
  linkRegexp = '';
  testText = '';
  protected activeTab = '0';
  protected pickerOpen = false;
  protected readonly testState = signal<TestState>({ kind: 'idle' });

  private lastQueue = '';
  private readonly testRequests = new Subject<void>();
  private readonly testSubscription: Subscription;

  constructor(private readonly api: RssApi) {
    // The server evaluates the expression, so the answer is the one the fetcher would give
    this.testSubscription = this.testRequests
      .pipe(
        debounceTime(250),
        switchMap(() => {
          if (this.testText === '') return of<TestState>({ kind: 'idle' });
          return this.api
            .test({
              expression: this.expression,
              text: this.testText,
              tvs: this.tvs,
              includeRepacks: this.includeRepacks,
              includeTrailers: this.includeTrailers,
            })
            .pipe(
              switchMap((result) => of<TestState>({ kind: 'result', result })),
              catchError(() => of<TestState>({ kind: 'invalid' })),
            );
        }),
      )
      .subscribe((state) => this.testState.set(state));
  }

  ngOnChanges(changes: SimpleChanges): void {
    if ('regexp' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
    }
  }

  ngOnDestroy(): void {
    this.testSubscription.unsubscribe();
  }

  private resetForm(): void {
    const r = this.regexp;
    const firstQueue = this.queues[0];
    this.feedId = r?.feedId || this.feeds[0]?.id || '';
    this.queue = r?.queue || firstQueue?.uuid || '';
    this.expression = r?.expression ?? '';
    // a new rule starts in the default directory of its queue, like the desktop dialog
    this.target = r?.target ?? this.queueDirectory(this.queue);
    this.addPaused = r?.addPaused ?? false;
    this.tvs = r?.tvs ?? 'none';
    this.from = r?.from ?? '';
    this.to = r?.to ?? '';
    if (r === null) {
      this.includeRepacks = true;
      this.includeTrailers = false;
      this.excludeManuals = true;
    } else {
      this.includeRepacks = r.includeRepacks;
      this.includeTrailers = r.includeTrailers;
      this.excludeManuals = r.excludeManuals;
    }
    this.extractLinks = (r?.linkRegexp ?? '') !== '';
    this.linkRegexp = r?.linkRegexp ?? '';
    this.testText = '';
    this.testState.set({ kind: 'idle' });
    this.activeTab = '0';
    this.pickerOpen = false;
    this.lastQueue = this.queue;
  }

  protected testResult(): RssTestResult | null {
    const state = this.testState();
    return state.kind === 'result' ? state.result : null;
  }

  protected get header(): string {
    return this.regexp === null ? 'Add RSS rule' : 'Edit RSS rule';
  }

  protected get episodeMode(): RssEpisodeMode | null {
    return this.tvs === 'none' ? null : this.tvs;
  }

  protected get example(): string {
    return this.episodeMode ? EPISODE_EXAMPLE[this.episodeMode] : '';
  }

  protected get rangeValid(): boolean {
    const mode = this.episodeMode;
    return (
      mode === null ||
      (isValidEpisode(mode, this.from, false) && isValidEpisode(mode, this.to, false))
    );
  }

  protected get valid(): boolean {
    return (
      this.feedId !== '' &&
      this.queue !== '' &&
      this.target.trim() !== '' &&
      this.rangeValid &&
      (!this.extractLinks || this.linkRegexp !== '')
    );
  }

  /** Managing the downloaded episodes works on the stored rule, so its mode must be the stored one. */
  protected get canManageDownloaded(): boolean {
    return this.regexp !== null && this.regexp.tvs !== 'none' && this.regexp.tvs === this.tvs;
  }

  protected queueDirectory(uuid: string): string {
    return this.queues.find((q) => q.uuid === uuid)?.defaultDirectory ?? '';
  }

  /** The directory follows the queue only while it is still the previous queue's default. */
  protected onQueueChange(): void {
    if (this.target === this.queueDirectory(this.lastQueue)) {
      this.target = this.queueDirectory(this.queue);
    }
    this.lastQueue = this.queue;
  }

  protected onTvsChange(): void {
    const range = defaultRange(this.tvs);
    this.from = range.from;
    this.to = range.to;
    this.scheduleTest();
  }

  protected scheduleTest(): void {
    this.testRequests.next();
  }

  protected onSave(): void {
    if (!this.valid) return;
    this.save.emit({
      feedId: this.feedId,
      queue: this.queue,
      expression: this.expression,
      target: this.target.trim(),
      addPaused: this.addPaused,
      tvs: this.tvs,
      from: this.from,
      to: this.to,
      includeRepacks: this.includeRepacks,
      includeTrailers: this.includeTrailers,
      excludeManuals: this.excludeManuals,
      linkRegexp: this.extractLinks ? this.linkRegexp : '',
    });
  }

  protected onManageDownloaded(): void {
    if (this.regexp !== null) this.manageDownloaded.emit(this.regexp);
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }
}
