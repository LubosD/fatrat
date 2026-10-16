export interface SpeedLimits {
  down: number;
  up: number;
}

export interface Stats {
  total: SpeedLimits;
  queues: { [queueUuid: string]: SpeedLimits };
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
