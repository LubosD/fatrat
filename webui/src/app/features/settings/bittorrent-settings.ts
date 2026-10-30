import { ChangeDetectorRef, Component, inject, ChangeDetectionStrategy } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { InputText } from '@openng/optimus-ui/inputtext';
import { InputNumber } from '@openng/optimus-ui/inputnumber';
import { Checkbox } from '@openng/optimus-ui/checkbox';
import { Select } from '@openng/optimus-ui/select';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { JsonValue } from '../../core/api/json-value';

const KEYS = [
  'torrent/listen_start',
  'torrent/listen_end',
  'torrent/maxratio',
  'torrent/maxconnections',
  'torrent/maxuploads',
  'torrent/maxconnections_loc',
  'torrent/maxuploads_loc',
  'torrent/maxfiles',
  'torrent/dht',
  'torrent/pex',
  'torrent/allocation',
  'torrent/external_ip',
  'torrent/enc_incoming',
  'torrent/enc_outgoing',
  'torrent/enc_level',
  'torrent/enc_rc4_prefer',
  'torrent/mapping_upnp',
  'torrent/mapping_natpmp',
  'torrent/mapping_lsd',
] as const;

@Component({
  selector: 'app-bittorrent-settings',
  imports: [FormsModule, InputText, InputNumber, Checkbox, Select],
  changeDetection: ChangeDetectionStrategy.Eager,
  templateUrl: './bittorrent-settings.html',
})
export class BittorrentSettings {
  private readonly transferApi = inject(TransferApi);
  private readonly draft = inject(SettingsDraftService);
  private readonly cdr = inject(ChangeDetectorRef);

  protected readonly allocationOptions = [
    { label: 'Full', value: 0 },
    { label: 'Sparse', value: 1 },
    { label: 'Compact', value: 2 },
  ];
  protected readonly encryptionModeOptions = [
    { label: 'Disabled', value: 0 },
    { label: 'Enabled', value: 1 },
    { label: 'Permit only encrypted', value: 2 },
  ];
  protected readonly encryptionLevelOptions = [
    { label: 'Plaintext', value: 0 },
    { label: 'RC4', value: 1 },
    { label: 'Both', value: 2 },
  ];

  protected listenStart = 0;
  protected listenEnd = 0;
  protected maxRatio = 0;
  protected maxConnectionsGlobal = 0;
  protected maxUploadsGlobal = 0;
  protected maxConnectionsPerTorrent = 0;
  protected maxUploadsPerTorrent = 0;
  protected maxOpenFiles = 0;
  protected dht = false;
  protected pex = false;
  protected allocation = 0;
  protected externalIp = '';
  protected encryptionIncoming = 0;
  protected encryptionOutgoing = 0;
  protected encryptionLevel = 0;
  protected encryptionPreferRc4 = false;
  protected portMappingUpnp = false;
  protected portMappingNatpmp = false;
  protected portMappingLsd = false;

  constructor() {
    this.transferApi.getSettingsValues([...KEYS]).subscribe({
      next: (values) => {
        this.applyLoadedValues(values);
        // The app is zoneless and these fields are plain properties, so tell Angular to re-render.
        this.cdr.markForCheck();
      },
      error: (err) => console.error('Failed to load BitTorrent settings', err),
    });
  }

  protected stage(key: string, value: JsonValue): void {
    this.draft.stage(key, value);
  }

  private applyLoadedValues(values: JsonValue[]): void {
    const raw = new Map<string, JsonValue>(KEYS.map((key, i) => [key, values[i]]));
    const get = (key: string): JsonValue | undefined => this.draft.get(key) ?? raw.get(key);
    const num = (key: string): number => Number(get(key) ?? 0);
    const bool = (key: string): boolean => Boolean(get(key) ?? false);

    this.listenStart = num('torrent/listen_start');
    this.listenEnd = num('torrent/listen_end');
    this.maxRatio = num('torrent/maxratio');
    this.maxConnectionsGlobal = num('torrent/maxconnections');
    this.maxUploadsGlobal = num('torrent/maxuploads');
    this.maxConnectionsPerTorrent = num('torrent/maxconnections_loc');
    this.maxUploadsPerTorrent = num('torrent/maxuploads_loc');
    this.maxOpenFiles = num('torrent/maxfiles');
    this.dht = bool('torrent/dht');
    this.pex = bool('torrent/pex');
    this.allocation = num('torrent/allocation');
    this.externalIp = String(get('torrent/external_ip') ?? '');
    this.encryptionIncoming = num('torrent/enc_incoming');
    this.encryptionOutgoing = num('torrent/enc_outgoing');
    this.encryptionLevel = num('torrent/enc_level');
    this.encryptionPreferRc4 = bool('torrent/enc_rc4_prefer');
    this.portMappingUpnp = bool('torrent/mapping_upnp');
    this.portMappingNatpmp = bool('torrent/mapping_natpmp');
    this.portMappingLsd = bool('torrent/mapping_lsd');
  }
}
