import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges, Type, inject, signal } from '@angular/core';
import { NgComponentOutlet } from '@angular/common';
import { Dialog } from 'primeng/dialog';
import { ButtonDirective } from 'primeng/button';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { SETTINGS_PAGE_COMPONENTS } from '../../core/settings-page-registry';
import { SettingsPage } from '../../core/api/models';

@Component({
  selector: 'app-settings-dialog',
  imports: [Dialog, ButtonDirective, NgComponentOutlet],
  providers: [SettingsDraftService],
  templateUrl: './settings-dialog.html',
})
export class SettingsDialog implements OnChanges {
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();

  private readonly transferApi = inject(TransferApi);
  private readonly draft = inject(SettingsDraftService);

  protected readonly pages = signal<SettingsPage[]>([]);
  protected readonly selectedPage = signal<SettingsPage | null>(null);

  /**
   * Bumped on every open so a response from a prior, already-closed dialog session can be
   * detected and ignored if it resolves after a newer session's response (rapid close/reopen).
   */
  private openSessionId = 0;

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['visible']?.currentValue === true) {
      this.draft.clear();
      const sessionId = ++this.openSessionId;
      this.transferApi.getSettingsPages().subscribe({
        next: (pages) => {
          if (sessionId !== this.openSessionId) {
            return;
          }
          this.pages.set(pages);
          this.selectedPage.set(pages.length > 0 ? pages[0] : null);
        },
        error: (err) => console.error('Failed to load settings pages', err),
      });
    }
  }

  protected selectPage(page: SettingsPage): void {
    this.selectedPage.set(page);
  }

  protected pageComponent(page: SettingsPage): Type<unknown> | null {
    return SETTINGS_PAGE_COMPONENTS[page.script] ?? null;
  }

  protected onOk(): void {
    this.transferApi.setSettingsValues(this.draft.getKeys(), this.draft.getValues()).subscribe({
      next: () => {
        this.transferApi.applySettings().subscribe({
          error: (err) => console.error('Failed to apply settings', err),
        });
      },
      error: (err) => console.error('Failed to save settings', err),
    });
    this.closeDialog();
  }

  protected onCancel(): void {
    this.closeDialog();
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }

  private closeDialog(): void {
    this.onDialogVisibleChange(false);
  }
}
