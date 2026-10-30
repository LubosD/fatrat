import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
  SimpleChanges,
  inject,
  signal,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from '@openng/optimus-ui/dialog';
import { InputText } from '@openng/optimus-ui/inputtext';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { RssApi } from '../../core/rss-api.service';
import { ApiError } from '../../core/api/rest-client';
import { RssRegexp } from '../../core/api/models';
import { EPISODE_EXAMPLE, RssEpisodeMode, isValidEpisode } from './rss-episode';

/** The episodes a rule already considers downloaded. Saves straight to the server. */
@Component({
  selector: 'app-rss-downloaded-dialog',
  imports: [FormsModule, Dialog, InputText, ButtonDirective],
  templateUrl: './rss-downloaded-dialog.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './rss-dialogs.scss',
})
export class RssDownloadedDialog implements OnChanges {
  private readonly api = inject(RssApi);

  @Input() regexp: RssRegexp | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();

  protected readonly episodes = signal<string[]>([]);
  protected readonly loading = signal(false);
  protected readonly error = signal('');
  protected newEpisode = '';

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['visible']?.currentValue === true && this.regexp !== null) {
      this.episodes.set([]);
      this.error.set('');
      this.newEpisode = '';
      this.loading.set(true);
      this.api.getDownloaded(this.regexp.id).subscribe({
        next: (list) => {
          this.episodes.set(list);
          this.loading.set(false);
        },
        error: (err: ApiError) => {
          this.error.set(err.message);
          this.loading.set(false);
        },
      });
    }
  }

  protected get mode(): RssEpisodeMode | null {
    const tvs = this.regexp?.tvs ?? 'none';
    return tvs === 'none' ? null : tvs;
  }

  protected get example(): string {
    return this.mode ? EPISODE_EXAMPLE[this.mode] : '';
  }

  protected get canAdd(): boolean {
    const value = this.newEpisode.trim();
    return (
      this.mode !== null &&
      isValidEpisode(this.mode, value, true) &&
      !this.episodes().includes(value)
    );
  }

  protected add(): void {
    if (!this.canAdd) return;
    this.episodes.update((list) => [...list, this.newEpisode.trim()]);
    this.newEpisode = '';
  }

  protected remove(episode: string): void {
    this.episodes.update((list) => list.filter((e) => e !== episode));
  }

  protected onSave(): void {
    if (this.regexp === null) return;
    this.api.setDownloaded(this.regexp.id, this.episodes()).subscribe({
      next: () => this.onDialogVisibleChange(false),
      error: (err: ApiError) => this.error.set(err.message),
    });
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }
}
