import { flattenFields, initialValues } from './form-values';
import { PluginFormField } from '../../core/api/models';

const fields: PluginFormField[] = [
  { type: 'label', title: 'Read me' },
  { type: 'text', key: 't', default: 'x' },
  { type: 'password', key: 'p' },
  { type: 'checkbox', key: 'c' },
  { type: 'combo', key: 'o', options: [{ value: 'a' }, { value: 'b' }] },
  { type: 'directory', key: 'd', default: '/tmp' },
  { type: 'list', key: 'l', rows: [{ value: 'r1' }, { value: 'r2' }] },
  { type: 'checklist', key: 'm', rows: [{ value: 'r1', checked: true }, { value: 'r2' }] },
  { type: 'group', key: 'g', title: 'G', items: [{ type: 'text', key: 'inner' }] },
  { type: 'group', title: 'Plain', items: [{ type: 'checkbox', key: 'deep', default: true }] },
];

describe('form values', () => {
  it('starts every keyed field with what the desktop would show', () => {
    expect(initialValues(fields)).toEqual({
      t: 'x', p: '', c: false, o: 'a', d: '/tmp', l: 'r1', m: ['r1'], g: false, inner: '', deep: true,
    });
  });

  it('flattens groups, turning a keyed group into its checkbox', () => {
    const flat = flattenFields(fields);
    expect(flat.map((f) => [f.field.type, f.field.key ?? null, f.group])).toEqual([
      ['label', null, ''],
      ['text', 't', ''],
      ['password', 'p', ''],
      ['checkbox', 'c', ''],
      ['combo', 'o', ''],
      ['directory', 'd', ''],
      ['list', 'l', ''],
      ['checklist', 'm', ''],
      ['checkbox', 'g', 'G'],
      ['text', 'inner', 'G'],
      ['checkbox', 'deep', 'Plain'],
    ]);
  });

  it('starts a number with its default, or 0', () => {
    expect(initialValues([
      { type: 'number', key: 'a' },
      { type: 'number', key: 'b', default: 2.5 },
    ])).toEqual({ a: 0, b: 2.5 });
  });

  it('starts a number inside its limits', () => {
    expect(initialValues([{ type: 'number', key: 'a', min: 1 }])).toEqual({ a: 1 });
  });
});
