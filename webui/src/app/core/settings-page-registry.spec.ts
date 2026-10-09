import { SETTINGS_PAGE_COMPONENTS } from './settings-page-registry';
import { BittorrentSettings } from '../features/settings/bittorrent-settings';
import { WebinterfaceSettings } from '../features/settings/webinterface-settings';

describe('SETTINGS_PAGE_COMPONENTS', () => {
  it('maps the BitTorrent and web interface script paths to their components', () => {
    expect(SETTINGS_PAGE_COMPONENTS['/scripts/settings/bittorrent.js']).toBe(BittorrentSettings);
    expect(SETTINGS_PAGE_COMPONENTS['/scripts/settings/webinterface.js']).toBe(WebinterfaceSettings);
  });

  it('has no entry for an unregistered page (e.g. the removed Jabber plugin)', () => {
    expect(SETTINGS_PAGE_COMPONENTS['/scripts/settings/jabber.js']).toBeUndefined();
  });
});
