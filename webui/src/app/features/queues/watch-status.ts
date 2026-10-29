import { WatchStatus } from '../../core/api/models';

/** What is wrong with a queue's watch folder, or null when nothing is. */
export function watchStatusProblem(status: WatchStatus | undefined): string | null {
  switch (status) {
    case 'missing':
      return "The folder doesn't exist. FatRat keeps checking and starts watching it once it appears.";
    case 'not_writable':
      return "FatRat can't write to this folder, so it can't mark files as processed. Nothing is picked up.";
    case 'duplicate':
      return 'Another queue already watches this folder, so this setting is ignored.';
    default:
      return null;
  }
}
