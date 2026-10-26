import { BROWSER_EXTENSION_PAGE, browserExtensionPageUrl } from './browser-extension';

const instanceOf = (url: string) => decodeURIComponent(url.split('#instance=')[1]);

describe('browserExtensionPageUrl', () => {
  it('points at the website handoff page', () => {
    expect(browserExtensionPageUrl('http://nas:2233/').startsWith(`${BROWSER_EXTENSION_PAGE}#instance=`)).toBe(true);
  });

  it('passes the web UI directory, without file name, query or fragment', () => {
    expect(instanceOf(browserExtensionPageUrl('http://nas:2233/'))).toBe('http://nas:2233/');
    expect(instanceOf(browserExtensionPageUrl('https://h/fatrat/index.html?x=1#add=y'))).toBe('https://h/fatrat/');
  });

  it('never passes credentials embedded in the address', () => {
    expect(instanceOf(browserExtensionPageUrl('http://admin:secret@nas:2233/'))).toBe('http://nas:2233/');
  });
});
