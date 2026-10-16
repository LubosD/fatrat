import { Type } from '@angular/core';
import { BittorrentSettings } from '../features/settings/bittorrent-settings';
import { WebinterfaceSettings } from '../features/settings/webinterface-settings';

/**
 * Keyed by SettingsPage.script, exactly as Settings.getPages() returns it. Pages with no entry
 * here render nothing extra in the dialog (a "not supported" message) — this will only happen if
 * the backend registers a new settings page this UI hasn't been updated to handle yet.
 */
export const SETTINGS_PAGE_COMPONENTS: Record<string, Type<unknown>> = {
  '/scripts/settings/bittorrent.js': BittorrentSettings,
  '/scripts/settings/webinterface.js': WebinterfaceSettings,
};
