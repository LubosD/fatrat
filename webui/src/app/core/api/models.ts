export interface SpeedLimits {
  down: number;
  up: number;
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
