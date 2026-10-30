import { watchStatusProblem } from './watch-status';

describe('watchStatusProblem', () => {
  it('is null when the folder works or is not set', () => {
    expect(watchStatusProblem('ok')).toBeNull();
    expect(watchStatusProblem('off')).toBeNull();
    expect(watchStatusProblem(undefined)).toBeNull();
  });

  it('explains each problem', () => {
    expect(watchStatusProblem('missing')).toContain("doesn't exist");
    expect(watchStatusProblem('not_writable')).toContain("can't write");
    expect(watchStatusProblem('duplicate')).toContain('Another queue');
  });
});
