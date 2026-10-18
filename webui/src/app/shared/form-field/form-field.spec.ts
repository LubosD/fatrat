import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { FormField } from './form-field';
import { PluginFormField } from '../../core/api/models';
import { TransferApi } from '../../core/api/transfer-api';
import { JsonValue } from '../../core/api/json-value';

function render(field: PluginFormField, value: JsonValue) {
  TestBed.configureTestingModule({
    imports: [FormField],
    providers: [
      { provide: TransferApi, useValue: { browseDirectories: () => of({ path: '/', parent: '', entries: [] }) } },
    ],
  });
  const fixture = TestBed.createComponent(FormField);
  fixture.componentRef.setInput('field', field);
  fixture.componentRef.setInput('value', value);
  fixture.componentRef.setInput('inputId', 'f1');
  const changes: JsonValue[] = [];
  fixture.componentInstance.value.subscribe((v) => changes.push(v));
  fixture.detectChanges();
  return { fixture, el: fixture.nativeElement as HTMLElement, changes };
}

describe('FormField', () => {
  it('renders a text field with its title and reports edits', async () => {
    const { fixture, el, changes } = render({ type: 'text', key: 'name', title: 'Name' }, 'Ann');
    await fixture.whenStable();
    const input = el.querySelector('input#f1') as HTMLInputElement;
    expect(el.querySelector('label')!.textContent).toContain('Name');
    expect(input.value).toBe('Ann');
    input.value = 'Bob';
    input.dispatchEvent(new Event('input'));
    expect(changes).toContain('Bob');
  });

  it('renders a password field as one', () => {
    const { el } = render({ type: 'password', key: 'p', title: 'Password' }, '');
    expect((el.querySelector('input#f1') as HTMLInputElement).type).toBe('password');
  });

  it('lets a label override the title', () => {
    const { fixture, el } = render({ type: 'text', key: 'k', title: 'Title' }, '');
    fixture.componentRef.setInput('label', 'Plugin — Title');
    fixture.detectChanges();
    expect(el.querySelector('label')!.textContent).toContain('Plugin — Title');
  });

  it('renders a label as plain text', () => {
    const { el } = render({ type: 'label', title: 'Just so you know' }, null);
    expect(el.querySelector('[data-testid="form-field-label"]')!.textContent).toContain('Just so you know');
    expect(el.querySelector('input')).toBeNull();
  });

  it('selects one row of a list', () => {
    const rows = [{ value: 'r1', cells: ['one'] }, { value: 'r2', cells: ['two'] }];
    const { fixture, changes } = render({ type: 'list', key: 'l', title: 'Pick', columns: ['Name'], rows }, 'r1');
    expect(fixture.componentInstance['selection']()).toEqual(rows[0]);
    fixture.componentInstance['onSelection'](rows[1]);
    expect(changes).toContain('r2');
  });

  it('selects any rows of a checklist', () => {
    const rows = [{ value: 'r1' }, { value: 'r2' }];
    const { fixture, changes } = render({ type: 'checklist', key: 'm', rows }, ['r2']);
    expect(fixture.componentInstance['selection']()).toEqual([rows[1]]);
    fixture.componentInstance['onSelection']([rows[0], rows[1]]);
    expect(changes).toContainEqual(['r1', 'r2']);
  });

  it('offers a server-side directory picker', () => {
    const { fixture, el } = render({ type: 'directory', key: 'd', title: 'Directory' }, '/tmp');
    (el.querySelector('[data-testid="form-field-browse"]') as HTMLButtonElement).click();
    fixture.detectChanges();
    expect(el.querySelector('app-directory-picker')).not.toBeNull();
  });
});
