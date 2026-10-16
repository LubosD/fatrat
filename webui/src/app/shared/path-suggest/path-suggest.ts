import { DestroyRef, Directive, ElementRef, HostListener, inject } from '@angular/core';
import { Subscription } from 'rxjs';
import { TransferApi } from '../../core/api/transfer-api';
import { DirectoryEntry } from '../../core/api/models';

const MAX_SUGGESTIONS = 50;
const DEBOUNCE_MS = 120;

/**
 * Turns a text input holding a server-side directory path into a combobox: while typing an absolute path,
 * the subdirectories of the part before the last '/' that match the rest are offered in a popup.
 * Up/Down choose, Enter (or click) completes the path and keeps going, Escape closes.
 * Works alongside ngModel by dispatching an `input` event after a completion.
 */
@Directive({
  selector: 'input[appPathSuggest]',
  host: {
    role: 'combobox',
    autocomplete: 'off',
    'aria-autocomplete': 'list',
    '[attr.aria-expanded]': 'open',
  },
})
export class PathSuggest {
  private readonly api = inject(TransferApi);
  private readonly input = inject<ElementRef<HTMLInputElement>>(ElementRef).nativeElement;

  private list: HTMLUListElement | null = null;
  private suggestions: DirectoryEntry[] = [];
  private active = -1;
  private timer: ReturnType<typeof setTimeout> | undefined;
  private request: Subscription | null = null;
  private cache: { dir: string; showHidden: boolean; entries: DirectoryEntry[] } | null = null;

  protected get open(): boolean {
    return this.list !== null;
  }

  constructor() {
    inject(DestroyRef).onDestroy(() => {
      clearTimeout(this.timer);
      this.request?.unsubscribe();
      this.close();
    });
  }

  @HostListener('input')
  protected onInput(): void {
    clearTimeout(this.timer);
    this.timer = setTimeout(() => this.refresh(), DEBOUNCE_MS);
  }

  @HostListener('blur')
  protected onBlur(): void {
    clearTimeout(this.timer);
    this.request?.unsubscribe();
    this.close();
  }

  @HostListener('keydown', ['$event'])
  protected onKeydown(event: KeyboardEvent): void {
    if (!this.open) {
      if (event.key === 'ArrowDown' && this.input.value.startsWith('/')) {
        event.preventDefault();
        this.refresh();
      }
      return;
    }
    switch (event.key) {
      case 'ArrowDown':
        event.preventDefault();
        this.move(1);
        break;
      case 'ArrowUp':
        event.preventDefault();
        this.move(-1);
        break;
      case 'Enter':
      case 'Tab':
        if (this.active >= 0) {
          event.preventDefault();
          event.stopPropagation();
          this.choose(this.suggestions[this.active]);
        }
        break;
      case 'Escape':
        // Close just the popup, not the dialog around the field.
        event.preventDefault();
        event.stopPropagation();
        this.close();
        break;
    }
  }

  private refresh(): void {
    const value = this.input.value;
    if (!value.startsWith('/') || document.activeElement !== this.input) {
      this.close();
      return;
    }
    const slash = value.lastIndexOf('/');
    const dir = slash === 0 ? '/' : value.slice(0, slash);
    const prefix = value.slice(slash + 1).toLowerCase();
    const showHidden = prefix.startsWith('.');

    if (this.cache?.dir === dir && this.cache.showHidden === showHidden) {
      this.show(this.cache.entries, prefix);
      return;
    }
    this.request?.unsubscribe();
    this.request = this.api.browseDirectories(dir, '', showHidden).subscribe({
      next: (listing) => {
        this.cache = { dir, showHidden, entries: listing.entries };
        // Typing may have moved on while the listing was in flight.
        if (this.input.value === value) this.show(listing.entries, prefix);
      },
      error: () => this.close(),
    });
  }

  private show(entries: DirectoryEntry[], prefix: string): void {
    this.suggestions = entries
      .filter((e) => e.name.toLowerCase().startsWith(prefix))
      // Already fully typed: nothing left to complete.
      .filter((e) => !(this.input.value === e.path))
      .slice(0, MAX_SUGGESTIONS);
    if (this.suggestions.length === 0) {
      this.close();
      return;
    }
    this.active = -1;
    this.render();
  }

  private choose(entry: DirectoryEntry): void {
    this.input.value = entry.path.endsWith('/') ? entry.path : entry.path + '/';
    this.input.dispatchEvent(new Event('input', { bubbles: true }));
    this.close();
    // Offer the next level right away.
    this.refresh();
  }

  private move(delta: number): void {
    const n = this.suggestions.length;
    this.active = this.active < 0 ? (delta > 0 ? 0 : n - 1) : (this.active + delta + n) % n;
    this.render();
  }

  private render(): void {
    if (!this.list) {
      const ul = document.createElement('ul');
      ul.setAttribute('role', 'listbox');
      ul.setAttribute('data-testid', 'path-suggest-list');
      ul.style.cssText =
        'position:fixed;z-index:100000;margin:0;padding:0.25rem 0;list-style:none;overflow:auto;max-height:14rem;' +
        'background:var(--p-overlay-select-background,#fff);color:var(--p-overlay-select-color,inherit);' +
        'border:1px solid var(--p-overlay-select-border-color,#ccc);border-radius:6px;box-shadow:0 4px 12px rgba(0,0,0,.2);';
      document.body.appendChild(ul);
      this.list = ul;
      this.input.setAttribute('aria-controls', 'path-suggest-list');
      ul.id = 'path-suggest-list';
    }
    const ul = this.list;
    const rect = this.input.getBoundingClientRect();
    ul.style.left = `${rect.left}px`;
    ul.style.top = `${rect.bottom + 2}px`;
    ul.style.minWidth = `${rect.width}px`;
    ul.replaceChildren();
    this.suggestions.forEach((entry, i) => {
      const li = document.createElement('li');
      li.id = `path-suggest-${i}`;
      li.setAttribute('role', 'option');
      li.setAttribute('data-testid', 'path-suggest-option');
      li.setAttribute('aria-selected', String(i === this.active));
      li.textContent = entry.name;
      li.style.cssText =
        'padding:0.35rem 0.75rem;cursor:pointer;white-space:nowrap;' +
        (i === this.active ? 'background:var(--p-highlight-background,#e8f0fe);' : '');
      // mousedown, not click: the input must keep focus or blur would close the list first.
      li.addEventListener('mousedown', (e) => {
        e.preventDefault();
        this.choose(entry);
      });
      ul.appendChild(li);
    });
    if (this.active >= 0) {
      this.input.setAttribute('aria-activedescendant', `path-suggest-${this.active}`);
      ul.children[this.active]?.scrollIntoView?.({ block: 'nearest' });
    } else {
      this.input.removeAttribute('aria-activedescendant');
    }
  }

  private close(): void {
    this.list?.remove();
    this.list = null;
    this.suggestions = [];
    this.active = -1;
    this.input.removeAttribute('aria-activedescendant');
    this.input.removeAttribute('aria-controls');
  }
}
