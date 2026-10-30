import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
  SimpleChanges,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from '@openng/optimus-ui/dialog';
import { InputText } from '@openng/optimus-ui/inputtext';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { RssFeed } from '../../core/api/models';

export interface RssFeedFormValue {
  name: string;
  url: string;
}

/**
 * Does not close itself on Save: the parent performs the request and either closes it or sets
 * `error` (a duplicate URL, say), so the user keeps what they typed.
 */
@Component({
  selector: 'app-rss-feed-dialog',
  imports: [FormsModule, Dialog, InputText, ButtonDirective],
  templateUrl: './rss-feed-dialog.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './rss-dialogs.scss',
})
export class RssFeedDialog implements OnChanges {
  @Input() feed: RssFeed | null = null;
  @Input() visible = false;
  @Input() error = '';
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<RssFeedFormValue>();

  name = '';
  url = '';

  ngOnChanges(changes: SimpleChanges): void {
    if ('feed' in changes || changes['visible']?.currentValue === true) {
      this.name = this.feed?.name ?? '';
      this.url = this.feed?.url ?? '';
    }
  }

  protected get header(): string {
    return this.feed === null ? 'Add RSS feed' : 'Edit RSS feed';
  }

  protected get valid(): boolean {
    return this.name.trim() !== '' && this.url.trim() !== '';
  }

  protected onSave(): void {
    if (this.valid) this.save.emit({ name: this.name.trim(), url: this.url.trim() });
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }
}
