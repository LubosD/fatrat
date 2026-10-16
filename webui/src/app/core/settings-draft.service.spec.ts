import { SettingsDraftService } from './settings-draft.service';

describe('SettingsDraftService', () => {
  it('returns undefined for a key that has not been staged', () => {
    const service = new SettingsDraftService();
    expect(service.get('torrent/maxratio')).toBeUndefined();
  });

  it('returns the staged value for a key after it has been staged', () => {
    const service = new SettingsDraftService();
    service.stage('torrent/maxratio', 2.5);
    expect(service.get('torrent/maxratio')).toBe(2.5);
  });

  it('overwrites a previously staged value for the same key', () => {
    const service = new SettingsDraftService();
    service.stage('torrent/maxratio', 2.5);
    service.stage('torrent/maxratio', 3);
    expect(service.get('torrent/maxratio')).toBe(3);
  });

  it('getKeys()/getValues() return everything staged, in staging order', () => {
    const service = new SettingsDraftService();
    service.stage('remote/port', 8080);
    service.stage('remote/enable', true);
    expect(service.getKeys()).toEqual(['remote/port', 'remote/enable']);
    expect(service.getValues()).toEqual([8080, true]);
  });

  it('clear() removes all staged entries', () => {
    const service = new SettingsDraftService();
    service.stage('remote/port', 8080);
    service.clear();
    expect(service.get('remote/port')).toBeUndefined();
    expect(service.getKeys()).toEqual([]);
  });
});
