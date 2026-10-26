import { parseAddFragment } from './incoming-links';

describe('parseAddFragment', () => {
  it('returns nothing for an empty or unrelated fragment', () => {
    expect(parseAddFragment('')).toEqual([]);
    expect(parseAddFragment('#')).toEqual([]);
    expect(parseAddFragment('#tab=2')).toEqual([]);
  });

  it('decodes every add value in order', () => {
    const magnet = 'magnet:?xt=urn:btih:abc&dn=A+B';
    const hash = `#add=${encodeURIComponent('http://a/1 x')}&add=${encodeURIComponent(magnet)}`;
    expect(parseAddFragment(hash)).toEqual(['http://a/1 x', magnet]);
  });

  it('accepts a fragment without the leading #', () => {
    expect(parseAddFragment(`add=${encodeURIComponent('http://a/1')}`)).toEqual(['http://a/1']);
  });

  it('drops blank values and trims whitespace', () => {
    expect(parseAddFragment('#add=&add=%20http%3A%2F%2Fa%2F1%20')).toEqual(['http://a/1']);
  });
});
