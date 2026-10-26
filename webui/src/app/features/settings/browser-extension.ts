/** Project website page that installs the browser extension, or hands an installed one our address. */
export const BROWSER_EXTENSION_PAGE = 'https://fatrat.dolezel.info/webext/';

export function browserExtensionPageUrl(pageHref: string): string {
  // '.' resolves to the directory and drops the query and fragment.
  const instance = new URL('.', pageHref);
  instance.username = '';
  instance.password = '';
  return `${BROWSER_EXTENSION_PAGE}#instance=${encodeURIComponent(instance.href)}`;
}
