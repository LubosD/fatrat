import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  OnDestroy,
  Output,
  SimpleChanges,
  inject,
  signal,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Subscription } from 'rxjs';
import { Dialog } from '@openng/optimus-ui/dialog';
import { InputText } from '@openng/optimus-ui/inputtext';
import { Checkbox } from '@openng/optimus-ui/checkbox';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { TransferApi } from '../../core/api/transfer-api';
import { DirectoryEntry } from '../../core/api/models';
import { PathSuggest } from '../path-suggest/path-suggest';

/**
 * Modal picker for a directory on the machine running FatRat. Navigates with the server-side
 * /filesystem/directories listing; the chosen path is emitted through `selected`.
 */
@Component({
  selector: 'app-directory-picker',
  imports: [FormsModule, Dialog, InputText, Checkbox, ButtonDirective, PathSuggest],
  templateUrl: './directory-picker.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './directory-picker.scss',
})
export class DirectoryPicker implements OnChanges, OnDestroy {
  private readonly api = inject(TransferApi);

  /** Where to start; empty lets the server choose (the queue's default directory, else home). */
  @Input() initialPath = '';
  @Input() queueUuid = '';
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() selected = new EventEmitter<string>();

  /** The directory currently listed (canonical, as reported by the server). */
  protected readonly path = signal('');
  protected readonly parent = signal('');
  protected readonly entries = signal<DirectoryEntry[]>([]);
  protected readonly loading = signal(false);
  protected readonly error = signal('');

  /** Text in the path field; applied on Enter / Go. */
  pathInput = '';
  showHidden = false;

  private request: Subscription | null = null;

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['visible']?.currentValue === true) {
      this.showHidden = false;
      this.path.set('');
      this.parent.set('');
      this.entries.set([]);
      this.load(this.initialPath);
    }
  }

  ngOnDestroy(): void {
    this.request?.unsubscribe();
  }

  protected navigate(path: string): void {
    this.load(path);
  }

  protected onGo(): void {
    this.load(this.pathInput.trim());
  }

  protected onToggleHidden(): void {
    this.load(this.path());
  }

  protected onSelect(): void {
    if (this.path() === '') return;
    this.selected.emit(this.path());
    this.closeDialog();
  }

  protected onCancel(): void {
    this.closeDialog();
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }

  private closeDialog(): void {
    this.onDialogVisibleChange(false);
  }

  private load(path: string): void {
    // A newer navigation supersedes an in-flight one, so a slow reply can't overwrite the listing.
    this.request?.unsubscribe();
    this.loading.set(true);
    this.error.set('');
    this.request = this.api.browseDirectories(path, this.queueUuid, this.showHidden).subscribe({
      next: (listing) => {
        this.path.set(listing.path);
        this.parent.set(listing.parent);
        this.entries.set(listing.entries);
        this.pathInput = listing.path;
        this.loading.set(false);
      },
      error: (err: unknown) => {
        const message =
          err instanceof Error && err.message ? err.message : 'Could not list the directory';
        if (this.path() === '' && path !== '') {
          // Nothing listed yet (e.g. a stale target typed earlier): start from the server's default instead.
          this.load('');
          this.error.set(`${message}: ${path}`);
          return;
        }
        // Keep the last good listing; only report what went wrong.
        this.error.set(message);
        this.pathInput = this.path();
        this.loading.set(false);
      },
    });
  }
}
