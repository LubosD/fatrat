/**
 * Links handed over by the browser extension arrive as `#add=<encoded URL>[&add=…]` on the web UI's
 * address. Values are percent-encoded, so URLSearchParams decodes them losslessly.
 */
export function parseAddFragment(hash: string): string[] {
  return new URLSearchParams(hash.replace(/^#/, ''))
    .getAll('add')
    .map((url) => url.trim())
    .filter((url) => url.length > 0);
}
