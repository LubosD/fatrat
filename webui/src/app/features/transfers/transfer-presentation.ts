import { Transfer, TransferState } from '../../core/api/models';

/** Icon file stems under img/states/, ported from the legacy web UI. Ready/Testing have none. */
const STATE_ICONS: Partial<Record<TransferState, string>> = {
  Waiting: 'waiting',
  Active: 'active',
  ForcedActive: 'forcedactive',
  Paused: 'paused',
  Failed: 'failed',
  Completed: 'completed',
};

const STATE_LABELS: Partial<Record<TransferState, string>> = {
  ForcedActive: 'Forced active',
};

/** Same rule as the legacy UI: a seeding/uploading transfer gets the upload variant. */
function isUploadIcon(transfer: Transfer): boolean {
  return transfer.mode === 'Upload' && (transfer.primaryMode === 'Upload' || transfer.state !== 'Completed');
}

export function stateIconPath(transfer: Transfer): string {
  return `img/states/${STATE_ICONS[transfer.state] ?? 'waiting'}${isUploadIcon(transfer) ? '_upload' : ''}.png`;
}

export function stateLabelText(transfer: Transfer): string {
  return STATE_LABELS[transfer.state] ?? transfer.state;
}

export function progressPercent(total: number, done: number): number {
  return total > 0 ? Math.min(100, Math.round((done / total) * 100)) : 0;
}
