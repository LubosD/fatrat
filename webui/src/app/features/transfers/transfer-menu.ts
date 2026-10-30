import type { MenuItem } from '@openng/optimus-ui/api';
import { AvailableTransferActions } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';

/** Shared by the table's context menu and the mobile cards' "more" menu. */
export function createTransferMenuItems(actions: TransferActionsService, onProperties: () => void): MenuItem[] {
  return [
    { id: 'resume', label: 'Resume', icon: 'pi pi-play', command: () => actions.resume() },
    { id: 'forceResume', label: 'Force Resume', icon: 'pi pi-forward', command: () => actions.forceResume() },
    { id: 'pause', label: 'Pause', icon: 'pi pi-pause', command: () => actions.pause() },
    { separator: true },
    { id: 'delete', label: 'Delete', icon: 'pi pi-trash', command: () => actions.deleteSelected(false) },
    { id: 'delete', label: 'Delete with Data', icon: 'pi pi-eraser', command: () => actions.deleteSelected(true) },
    { separator: true },
    { id: 'moveUp', label: 'Move to Top', icon: 'pi pi-angle-double-up', command: () => actions.moveSelected('top') },
    { id: 'moveUp', label: 'Move Up', icon: 'pi pi-angle-up', command: () => actions.moveSelected('up') },
    { id: 'moveDown', label: 'Move Down', icon: 'pi pi-angle-down', command: () => actions.moveSelected('down') },
    { id: 'moveDown', label: 'Move to Bottom', icon: 'pi pi-angle-double-down', command: () => actions.moveSelected('bottom') },
    { separator: true },
    { id: 'properties', label: 'Properties', icon: 'pi pi-pencil', command: () => onProperties() },
  ];
}

/** A fresh array, so the menu re-renders the new disabled flags. */
export function withAvailability(items: MenuItem[], available: AvailableTransferActions): MenuItem[] {
  return items.map((item) => (item.id ? { ...item, disabled: !available[item.id as keyof AvailableTransferActions] } : item));
}
