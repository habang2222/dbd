using UnityEngine;

public static class InventoryService
{
    public static int GetItemCount(PersonComponent owner, string itemId)
    {
        return owner != null && owner.Inventory != null
            ? owner.Inventory.GetItemCount(itemId)
            : 0;
    }

    public static bool CanRemoveItem(PersonComponent owner, string itemId, int count)
    {
        return owner != null && owner.Inventory != null && owner.Inventory.CanRemoveItem(itemId, count);
    }

    public static bool AddItem(PersonComponent owner, string itemId, int count)
    {
        if (!ServerItemGrantLedger.IsServerItemGrantActive)
        {
            AntiCheatService.Punish(owner, "item created outside server grant");
            Debug.LogWarning($"Inventory add rejected for {(owner != null ? owner.PersonName : "unknown")}: item created outside server grant");
            return false;
        }

        if (!ServerItemGrantLedger.ConsumeGrant(owner, itemId, count))
        {
            AntiCheatService.Punish(owner, "item grant proof missing");
            Debug.LogWarning($"Inventory add rejected for {(owner != null ? owner.PersonName : "unknown")}: item grant proof missing");
            return false;
        }

        if (!AntiCheatService.CanAcceptInventoryAdd(owner, itemId, count, out string reason) || owner.Inventory == null)
        {
            Debug.LogWarning($"Inventory add rejected for {(owner != null ? owner.PersonName : "unknown")}: {reason}");
            return false;
        }

        GetMutationTracker(owner).AllowChange(itemId, count);
        owner.Inventory.AddItem(itemId, count);
        RefreshLocalInventoryWindows();
        return true;
    }

    public static bool RemoveItem(PersonComponent owner, string itemId, int count)
    {
        if (!AntiCheatService.CanAcceptInventoryRemove(owner, itemId, count, out string reason) || owner.Inventory == null)
        {
            Debug.LogWarning($"Inventory remove rejected for {(owner != null ? owner.PersonName : "unknown")}: {reason}");
            return false;
        }

        GetMutationTracker(owner).AllowChange(itemId, -count);
        bool removed = owner.Inventory.RemoveItem(itemId, count);
        if (removed)
        {
            RefreshLocalInventoryWindows();
        }

        return removed;
    }

    private static void RefreshLocalInventoryWindows()
    {
        OwnedItemsWindow.Instance?.Refresh();
    }

    private static InventoryMutationTracker GetMutationTracker(PersonComponent owner)
    {
        InventoryMutationTracker tracker = owner.GetComponent<InventoryMutationTracker>();
        if (tracker == null)
        {
            tracker = owner.gameObject.AddComponent<InventoryMutationTracker>();
        }

        return tracker;
    }
}
