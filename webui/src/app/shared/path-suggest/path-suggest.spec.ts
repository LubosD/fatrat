import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { FormsModule } from '@angular/forms';
import { of, throwError } from 'rxjs';
import { PathSuggest } from './path-suggest';
import { TransferApi } from '../../core/api/transfer-api';
import { DirectoryListing } from '../../core/api/models';

@Component({
  imports: [FormsModule, PathSuggest],
  template: `<input appPathSuggest [(ngModel)]="value" />`,
})
class Host {
  value = '';
}

const tree: Record<string, string[]> = { '/': ['home', 'hdd', 'tmp'], '/home': ['alice', 'bob'], '/home/alice': ['dl'] };

async function setup() {
  vi.useFakeTimers();
  const browseDirectories = vi.fn((path?: string): ReturnType<TransferApi['browseDirectories']> => {
    const names = tree[path ?? ''];
    if (!names) return throwError(() => new Error('nope'));
    const base = path === '/' ? '' : path;
    return of({ path: path!, parent: '', entries: names.map((n) => ({ name: n, path: `${base}/${n}` })) } as DirectoryListing);
  });
  TestBed.configureTestingModule({ providers: [{ provide: TransferApi, useValue: { browseDirectories } }] });
  const fixture = TestBed.createComponent(Host);
  fixture.detectChanges();
  const input = fixture.nativeElement.querySelector('input') as HTMLInputElement;
  input.focus();
  return { fixture, input, browseDirectories };
}

function type(input: HTMLInputElement, text: string) {
  input.value = text;
  input.dispatchEvent(new Event('input', { bubbles: true }));
  vi.advanceTimersByTime(200);
}

function key(input: HTMLInputElement, k: string) {
  const e = new KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true });
  input.dispatchEvent(e);
  return e;
}

function options(): string[] {
  return Array.from(document.querySelectorAll('[data-testid="path-suggest-option"]')).map((o) => o.textContent!);
}

describe('PathSuggest', () => {
  afterEach(() => vi.useRealTimers());

  it('suggests matching subdirectories of the typed parent', async () => {
    const { input, browseDirectories } = await setup();
    type(input, '/h');
    expect(browseDirectories).toHaveBeenCalledWith('/', '', false);
    expect(options()).toEqual(['home', 'hdd']);
  });

  it('chooses with arrow keys + Enter, updates the model and continues one level down', async () => {
    const { fixture, input } = await setup();
    type(input, '/ho');
    key(input, 'ArrowDown');
    const enter = key(input, 'Enter');
    fixture.detectChanges();
    await fixture.whenStable();

    expect(enter.defaultPrevented).toBe(true);
    expect(input.value).toBe('/home/');
    expect(fixture.componentInstance.value).toBe('/home/');
    vi.advanceTimersByTime(200);
    expect(options()).toEqual(['alice', 'bob']);
  });

  it('lets Enter through when nothing is highlighted, and closes on Escape', async () => {
    const { input } = await setup();
    type(input, '/h');
    expect(key(input, 'Enter').defaultPrevented).toBe(false);
    expect(key(input, 'Escape').defaultPrevented).toBe(true);
    expect(options()).toEqual([]);
  });

  it('offers nothing for relative paths or unreadable directories', async () => {
    const { input, browseDirectories } = await setup();
    type(input, 'home');
    expect(browseDirectories).not.toHaveBeenCalled();
    type(input, '/missing/x');
    expect(options()).toEqual([]);
  });
});
