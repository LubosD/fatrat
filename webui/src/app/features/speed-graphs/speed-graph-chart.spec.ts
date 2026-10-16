import { TestBed } from '@angular/core/testing';
import { SpeedGraphChart } from './speed-graph-chart';

describe('SpeedGraphChart', () => {
  it('shows a "no data" message when there are no samples', () => {
    TestBed.configureTestingModule({ imports: [SpeedGraphChart] });
    const fixture = TestBed.createComponent(SpeedGraphChart);
    fixture.componentRef.setInput('samples', []);
    fixture.componentRef.setInput('title', 'Transfer Speed');
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('No data yet');
    expect(fixture.nativeElement.querySelector('p-chart')).toBeNull();
  });

  it('renders a chart with download/upload datasets when samples are present', () => {
    TestBed.configureTestingModule({ imports: [SpeedGraphChart] });
    const fixture = TestBed.createComponent(SpeedGraphChart);
    fixture.componentRef.setInput('samples', [
      { down: 100, up: 50 },
      { down: 200, up: 75 },
    ]);
    fixture.componentRef.setInput('title', 'Transfer Speed');
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('p-chart')).toBeTruthy();
    const chartData = fixture.componentInstance['chartData'];
    expect(chartData.datasets).toHaveLength(2);
    expect(chartData.datasets[0].data).toEqual([100, 200]);
    expect(chartData.datasets[1].data).toEqual([50, 75]);
  });

  it('shows the title as a legend regardless of data state', () => {
    TestBed.configureTestingModule({ imports: [SpeedGraphChart] });
    const fixture = TestBed.createComponent(SpeedGraphChart);
    fixture.componentRef.setInput('samples', []);
    fixture.componentRef.setInput('title', 'Queue Speed');
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('legend').textContent).toContain('Queue Speed');
  });
});
