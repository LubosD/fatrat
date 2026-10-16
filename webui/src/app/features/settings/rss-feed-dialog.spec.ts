import { TestBed } from '@angular/core/testing';
import { RssFeedDialog } from './rss-feed-dialog';

function create(feed: { id: string; name: string; url: string } | null) {
  TestBed.configureTestingModule({ imports: [RssFeedDialog] });
  const fixture = TestBed.createComponent(RssFeedDialog);
  fixture.componentRef.setInput('feed', feed);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
  return fixture;
}

describe('RssFeedDialog', () => {
  it('starts blank for a new feed and is prefilled when editing', () => {
    let fixture = create(null);
    expect(fixture.componentInstance.name).toBe('');
    expect(fixture.nativeElement.textContent).toContain('Add RSS feed');
    TestBed.resetTestingModule();

    fixture = create({ id: 'f1', name: 'News', url: 'http://n/rss' });
    expect(fixture.componentInstance.name).toBe('News');
    expect(fixture.componentInstance.url).toBe('http://n/rss');
    expect(fixture.nativeElement.textContent).toContain('Edit RSS feed');
  });

  it('does not emit save while the name or the URL is empty', () => {
    const fixture = create(null);
    const save = vi.fn();
    fixture.componentInstance.save.subscribe(save);

    fixture.componentInstance.name = 'x';
    fixture.componentInstance['onSave']();
    fixture.componentInstance.name = '  ';
    fixture.componentInstance.url = 'http://x';
    fixture.componentInstance['onSave']();

    expect(save).not.toHaveBeenCalled();
  });

  it('emits trimmed values on save and stays open so the parent can report an error', () => {
    const fixture = create(null);
    const save = vi.fn();
    const visibleChange = vi.fn();
    fixture.componentInstance.save.subscribe(save);
    fixture.componentInstance.visibleChange.subscribe(visibleChange);

    fixture.componentInstance.name = ' News ';
    fixture.componentInstance.url = ' http://n/rss ';
    fixture.componentInstance['onSave']();

    expect(save).toHaveBeenCalledWith({ name: 'News', url: 'http://n/rss' });
    expect(visibleChange).not.toHaveBeenCalled();
  });

  it('shows the error handed in by the parent', () => {
    const fixture = create(null);
    fixture.componentRef.setInput('error', 'A feed with this URL already exists');
    fixture.detectChanges();
    expect(document.body.textContent).toContain('A feed with this URL already exists');
  });
});
