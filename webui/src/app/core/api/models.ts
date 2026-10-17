export interface SpeedLimits {
  down: number;
  up: number;
}

export interface Stats {
  total: SpeedLimits;
  queues: { [queueUuid: string]: SpeedLimits };
}

export interface DeleteQueueOptions {
  /** Move the queue's transfers here; mutually exclusive with deleteTransfers. */
  moveTo?: string;
  deleteTransfers?: boolean;
  withData?: boolean;
}

export interface Queue {
  uuid: string;
  name: string;
  defaultDirectory: string;
  moveDirectory: string;
  upAsDown: boolean;
  transferLimits: SpeedLimits;
  speedLimits: SpeedLimits;
}

export type TransferState =
  | 'Waiting'
  | 'Active'
  | 'Paused'
  | 'ForcedActive'
  | 'Failed'
  | 'Completed'
  | 'Ready'
  | 'Testing';

export type TransferMode = 'Download' | 'Upload';

export interface Transfer {
  uuid: string;
  name: string;
  state: TransferState;
  class: string;
  message: string;
  mode: TransferMode;
  primaryMode: TransferMode;
  dataPath: string;
  dataPathIsDir: boolean;
  total: number;
  done: number;
  comment: string;
  object: string;
  timeRunning: number;
  speeds: SpeedLimits;
  userSpeedLimits: SpeedLimits;
  detailsScript?: string;
}

export interface TransferClass {
  mode: TransferMode;
  shortName: string;
  longName: string;
}

export interface DirectoryEntry {
  name: string;
  path: string;
}

export interface DirectoryListing {
  path: string;
  parent: string;
  entries: DirectoryEntry[];
}

export interface SettingsPage {
  title: string;
  script: string;
  icon: string;
}

export interface SpeedSample {
  down: number;
  up: number;
}

export interface NewTransferOptions {
  upload: boolean;
  queueUuid: string;
  targetClass: string;
  target: string;
  paused: boolean;
  speedLimits: SpeedLimits;
}

export interface RssFeed {
  id: string;
  name: string;
  url: string;
}

/** How episodes are numbered for duplicate detection: S01E02, 0123 or 2026-10-16. */
export type RssTvsMode = 'none' | 'season' | 'episode' | 'date';

export interface RssRegexp {
  id: string;
  /** Empty if the stored rule refers to a feed that no longer exists. */
  feedId: string;
  expression: string;
  queue: string;
  target: string;
  addPaused: boolean;
  tvs: RssTvsMode;
  from: string;
  to: string;
  includeRepacks: boolean;
  includeTrailers: boolean;
  excludeManuals: boolean;
  linkRegexp: string;
}

export type RssRegexpProperties = Partial<Omit<RssRegexp, 'id'>>;

export interface RssTestResult {
  matches: boolean;
  episode?: string;
}

export type ScheduledActionType = 'resume' | 'stop' | 'speedLimit';

/** Times are local to the server. */
export interface ScheduledAction {
  id: string;
  name: string;
  queue: string;
  action: ScheduledActionType;
  /** Bytes/s. Only present for the speedLimit action. */
  speedLimit?: SpeedLimits;
  repeated: boolean;
  /** HH:mm. Applies if repeated; may be empty for a stored action that never had one. */
  time: string;
  /** Monday first. Applies if repeated. */
  days: boolean[];
  /** yyyy-MM-ddTHH:mm. Applies if not repeated; may be empty. */
  at: string;
}

export type ScheduledActionProperties = Partial<Omit<ScheduledAction, 'id'>>;

/** One item of a plugin's settings page, as declared in its plugin.json. */
export interface PluginConfigItem {
  type: 'text' | 'password' | 'checkbox' | 'combo' | 'order' | 'group';
  /** The settings key this item reads and writes. Absent on a plain group. */
  key?: string;
  title?: string;
  /** For combo and order. */
  options?: { value: string; title?: string }[];
  /** For group. */
  items?: PluginConfigItem[];
}

/** An installed plugin, as its manifest describes it. */
export interface Plugin {
  name: string;
  /** False when the manifest could not be read; then only name and error are set. */
  valid: boolean;
  error?: string;
  version?: string;
  description?: string;
  license?: string;
  homepage?: string;
  api?: string;
  /** Disabled plugins stay installed but are not loaded at the next start. */
  enabled?: boolean;
  config?: PluginConfigItem[];
  permissions?: string[];
  /** Id of the repository it was installed from, when known. */
  repository?: string;
  source?: string;
}

/** A plugin repository the user has added. */
export interface PluginRepository {
  id: string;
  source: string;
  /** Local repositories are used in place rather than fetched. */
  local: boolean;
  name: string;
  description: string;
  /** The commit FatRat last fetched; empty for a local repository. */
  revision: string;
}

/** A plugin one of the configured repositories offers. */
export interface AvailablePlugin {
  name: string;
  version: string;
  description: string;
  repository: string;
  /** Empty when not installed; different from version when an update is available. */
  installedVersion: string;
}
