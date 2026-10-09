import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { BittorrentSettings } from './bittorrent-settings';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';

const KEYS = [
  'torrent/listen_start', 'torrent/listen_end', 'torrent/maxratio', 'torrent/maxconnections',
  'torrent/maxuploads', 'torrent/maxconnections_loc', 'torrent/maxuploads_loc', 'torrent/maxfiles',
  'torrent/dht', 'torrent/pex', 'torrent/allocation', 'torrent/external_ip', 'torrent/enc_incoming',
  'torrent/enc_outgoing', 'torrent/enc_level', 'torrent/enc_rc4_prefer', 'torrent/mapping_upnp',
  'torrent/mapping_natpmp', 'torrent/mapping_lsd',
];

function makeValues(overrides: Record<string, unknown> = {}): unknown[] {
  const defaults: Record<string, unknown> = {
    'torrent/listen_start': 6881,
    'torrent/listen_end': 6889,
    'torrent/maxratio': 2,
    'torrent/maxconnections': 200,
    'torrent/maxuploads': 4,
    'torrent/maxconnections_loc': 50,
    'torrent/maxuploads_loc': 2,
    'torrent/maxfiles': 50,
    'torrent/dht': true,
    'torrent/pex': true,
    'torrent/allocation': 1,
    'torrent/external_ip': '',
    'torrent/enc_incoming': 1,
    'torrent/enc_outgoing': 1,
    'torrent/enc_level': 2,
    'torrent/enc_rc4_prefer': false,
    'torrent/mapping_upnp': true,
    'torrent/mapping_natpmp': true,
    'torrent/mapping_lsd': true,
    ...overrides,
  };
  return KEYS.map((key) => defaults[key]);
}

function configure(getSettingsValues = vi.fn().mockReturnValue(of(makeValues())), draftGet = vi.fn().mockReturnValue(undefined)) {
  const stage = vi.fn();
  TestBed.configureTestingModule({
    imports: [BittorrentSettings],
    providers: [
      { provide: TransferApi, useValue: { getSettingsValues } },
      { provide: SettingsDraftService, useValue: { get: draftGet, stage } },
    ],
  });
  return { getSettingsValues, stage, draftGet };
}

describe('BittorrentSettings', () => {
  it('fetches all the expected keys on construction', () => {
    const { getSettingsValues } = configure();
    TestBed.createComponent(BittorrentSettings).detectChanges();

    expect(getSettingsValues).toHaveBeenCalledWith(KEYS);
  });

  it('populates fields from the fetched values, using the correct (fixed) ratio key', () => {
    const { } = configure(vi.fn().mockReturnValue(of(makeValues({ 'torrent/maxratio': 1.5 }))));
    const fixture = TestBed.createComponent(BittorrentSettings);
    fixture.detectChanges();

    expect(fixture.componentInstance['maxRatio']).toBe(1.5);
    expect(fixture.componentInstance['listenStart']).toBe(6881);
    expect(fixture.componentInstance['dht']).toBe(true);
  });

  it('prefers a staged draft value over the fetched value', () => {
    const draftGet = vi.fn().mockImplementation((key: string) => (key === 'torrent/maxratio' ? 9.9 : undefined));
    configure(vi.fn().mockReturnValue(of(makeValues({ 'torrent/maxratio': 1.5 }))), draftGet);
    const fixture = TestBed.createComponent(BittorrentSettings);
    fixture.detectChanges();

    expect(fixture.componentInstance['maxRatio']).toBe(9.9);
  });

  it('stages a field change under the correct (fixed) key', () => {
    const { stage } = configure();
    const fixture = TestBed.createComponent(BittorrentSettings);
    fixture.detectChanges();

    fixture.componentInstance['stage']('torrent/maxratio', 3.5);

    expect(stage).toHaveBeenCalledWith('torrent/maxratio', 3.5);
  });
});
