import { TestBed } from '@angular/core/testing';
import { Observable, Subject, of, throwError } from 'rxjs';
import { DirectoryPicker } from './directory-picker';
import { TransferApi } from '../../core/api/transfer-api';
import { DirectoryListing } from '../../core/api/models';

function listing(path: string, names: string[] = [], parent = ''): DirectoryListing {
  return { path, parent, entries: names.map((n) => ({ name: n, path: `${path === '/' ? '' : path}/${n}` })) };
}

function setup(browse: (path?: string, queue?: string, hidden?: boolean) => Observable<DirectoryListing>) {
  const browseDirectories = vi.fn(browse);
  TestBed.configureTestingModule({
    imports: [DirectoryPicker],
    providers: [{ provide: TransferApi, useValue: { browseDirectories } }],
  });
  const fixture = TestBed.createComponent(DirectoryPicker);
  return { fixture, browseDirectories, el: fixture.nativeElement as HTMLElement };
}

function open(fixture: ReturnType<typeof setup>['fixture'], initialPath = '', queueUuid = '') {
  fixture.componentRef.setInput('initialPath', initialPath);
  fixture.componentRef.setInput('queueUuid', queueUuid);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
}

function entries(el: HTMLElement): string[] {
  return Array.from(el.querySelectorAll('[data-testid="directory-picker-entry"]')).map((e) => e.textContent!.trim());
}

function click(el: HTMLElement, testid: string, index = 0): void {
  (el.querySelectorAll(`[data-testid="${testid}"]`)[index] as HTMLElement).click();
}

describe('DirectoryPicker', () => {
  it('loads the initial path and the queue when opened and lists subdirectories', async () => {
    const { fixture, browseDirectories, el } = setup(() => of(listing('/home/u', ['a', 'b'], '/home')));
    open(fixture, '/home/u', 'q1');
    await fixture.whenStable();

    expect(browseDirectories).toHaveBeenCalledWith('/home/u', 'q1', false);
    expect(entries(el)).toEqual(['a', 'b']);
    expect((el.querySelector('[data-testid="directory-picker-path"]') as HTMLInputElement).value).toBe('/home/u');
  });

  it('navigates into a directory and back up', () => {
    const tree: Record<string, DirectoryListing> = {
      '/home/u': listing('/home/u', ['a'], '/home'),
      '/home/u/a': listing('/home/u/a', ['x'], '/home/u'),
      '/home': listing('/home', ['u'], '/'),
    };
    const { fixture, browseDirectories, el } = setup((p) => of(tree[p ?? '']));
    open(fixture, '/home/u');

    click(el, 'directory-picker-entry');
    fixture.detectChanges();
    expect(browseDirectories).toHaveBeenLastCalledWith('/home/u/a', '', false);
    expect(entries(el)).toEqual(['x']);

    click(el, 'directory-picker-up');
    fixture.detectChanges();
    expect(browseDirectories).toHaveBeenLastCalledWith('/home/u', '', false);
  });

  it('disables Up at the filesystem root', () => {
    const { fixture, el } = setup(() => of(listing('/', ['etc'], '')));
    open(fixture);
    expect((el.querySelector('[data-testid="directory-picker-up"]') as HTMLButtonElement).disabled).toBe(true);
  });

  it('navigates to a typed path on Go', () => {
    const { fixture, browseDirectories, el } = setup((p) => of(listing(p || '/home/u')));
    open(fixture);
    const input = el.querySelector('[data-testid="directory-picker-path"]') as HTMLInputElement;
    input.value = ' /mnt/data ';
    input.dispatchEvent(new Event('input'));
    click(el, 'directory-picker-go');
    expect(browseDirectories).toHaveBeenLastCalledWith('/mnt/data', '', false);
  });

  it('reloads the current directory when "show hidden" is toggled', () => {
    const { fixture, browseDirectories } = setup((p) => of(listing(p || '/home/u')));
    open(fixture);
    fixture.componentInstance.showHidden = true;
    (fixture.componentInstance as unknown as { onToggleHidden(): void }).onToggleHidden();
    expect(browseDirectories).toHaveBeenLastCalledWith('/home/u', '', true);
  });

  it('keeps the last good listing and shows the error when navigation fails', async () => {
    const { fixture, el } = setup((p) =>
      p === '/home/u/secret' ? throwError(() => new Error('Permission denied')) : of(listing('/home/u', ['secret'], '/home')),
    );
    open(fixture, '/home/u');

    click(el, 'directory-picker-entry');
    fixture.detectChanges();
    await fixture.whenStable();
    expect(el.querySelector('[data-testid="directory-picker-error"]')!.textContent).toContain('Permission denied');
    expect(entries(el)).toEqual(['secret']);
    expect((el.querySelector('[data-testid="directory-picker-path"]') as HTMLInputElement).value).toBe('/home/u');
  });

  it('falls back to the server default when the initial path cannot be listed', () => {
    const { fixture, browseDirectories, el } = setup((p) =>
      p === '/gone' ? throwError(() => new Error('No such directory')) : of(listing('/home/u', ['a'], '/home')),
    );
    open(fixture, '/gone');

    expect(browseDirectories).toHaveBeenLastCalledWith('', '', false);
    fixture.detectChanges();
    expect(entries(el)).toEqual(['a']);
    expect(el.querySelector('[data-testid="directory-picker-error"]')!.textContent).toContain('/gone');
  });

  it('ignores a superseded reply', () => {
    const slow = new Subject<DirectoryListing>();
    const { fixture, el } = setup((p) => (p === '/slow' ? slow : of(listing('/fast', ['f']))));
    open(fixture, '/slow');
    click(el, 'directory-picker-go'); // pathInput is still empty -> loads '' (server default)
    slow.next(listing('/slow', ['s']));
    fixture.detectChanges();
    expect(entries(el)).toEqual(['f']);
  });

  it('emits the listed path on Select and closes', () => {
    const { fixture, el } = setup(() => of(listing('/home/u')));
    const selected = vi.fn();
    const visibleChange = vi.fn();
    fixture.componentInstance.selected.subscribe(selected);
    fixture.componentInstance.visibleChange.subscribe(visibleChange);
    open(fixture);

    click(el, 'directory-picker-select');
    expect(selected).toHaveBeenCalledWith('/home/u');
    expect(visibleChange).toHaveBeenCalledWith(false);
  });

  it('closes without emitting on Cancel', () => {
    const { fixture, el } = setup(() => of(listing('/home/u')));
    const selected = vi.fn();
    const visibleChange = vi.fn();
    fixture.componentInstance.selected.subscribe(selected);
    fixture.componentInstance.visibleChange.subscribe(visibleChange);
    open(fixture);

    click(el, 'directory-picker-cancel');
    expect(selected).not.toHaveBeenCalled();
    expect(visibleChange).toHaveBeenCalledWith(false);
  });
});
