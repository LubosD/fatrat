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

  it('renders a number with its unit and reports edits as numbers', async () => {
    const { fixture, el, changes } = render(
      { type: 'number', key: 'n', title: 'Limit', min: 0, max: 10, decimals: 2, suffix: 'MB' },
      1.5,
    );
    await fixture.whenStable();
    fixture.detectChanges();
    const input = el.querySelector('p-inputnumber input') as HTMLInputElement;
    expect(el.querySelector('label')!.textContent).toContain('Limit');
    expect(input.id).toBe('f1');
    expect(input.value).toMatch(/1[.,]50?/);
    expect(input.value).toContain('MB');
    fixture.componentInstance['onNumber'](3);
    expect(changes).toContain(3);
  });

  it('turns a cleared number into 0, never null', () => {
    const { fixture, changes } = render({ type: 'number', key: 'n', title: 'Limit' }, 4);
    fixture.componentInstance['onNumber'](null);
    expect(changes).toContain(0);
  });

  it('turns a cleared number into the lower limit', () => {
    const { fixture, changes } = render({ type: 'number', key: 'n', title: 'Limit', min: 1 }, 4);
    fixture.componentInstance['onNumber'](null);
    expect(changes).toContain(1);
  });

  it('reports what is typed into the real input, and passes the limits on', async () => {
    const { fixture, el, changes } = render({ type: 'number', key: 'n', title: 'Limit', min: 0, max: 10 }, 1);
    await fixture.whenStable();
    fixture.detectChanges();
    const input = el.querySelector('p-inputnumber input') as HTMLInputElement;
    expect(input.getAttribute('aria-valuemin')).toBe('0');
    expect(input.getAttribute('aria-valuemax')).toBe('10');
    input.value = '7';
    input.dispatchEvent(new Event('input'));
    input.dispatchEvent(new Event('blur'));
    await fixture.whenStable();
    expect(changes).toContain(7);
  });
});
