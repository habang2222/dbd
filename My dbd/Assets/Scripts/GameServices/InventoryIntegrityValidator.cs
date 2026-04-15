using System.Collections.Generic;
using UnityEngine;

public class InventoryIntegrityValidator : MonoBehaviour
{
    private const int MaxStackCount = 10000;
    private const int MaxInventorySlots = 300;
    private readonly Dictionary<string, int> lastCounts = new();
    private bool hasSnapshot;

    private void LateUpdate()
    {
        PersonComponent person = GetComponent<PersonComponent>();
        if (person == null || person.Inventory == null || AntiCheatService.IsFrozen(person))
        {
            return;
        }

        if (person.Inventory.items.Count > MaxInventorySlots)
        {
            AntiCheatService.Punish(person, "too many inventory slots");
            return;
        }

        Dictionary<string, int> currentCounts = new();
        foreach (PersonInventoryItem item in person.Inventory.items)
        {
            if (item == null || string.IsNullOrWhiteSpace(item.itemId) || item.count < 0 || item.count > MaxStackCount)
            {
                AntiCheatService.Punish(person, "invalid inventory entry");
                return;
            }

            currentCounts.TryGetValue(item.itemId, out int current);
            currentCounts[item.itemId] = current + item.count;
        }

        foreach (KeyValuePair<string, int> pair in currentCounts)
        {
            if (pair.Value > MaxStackCount)
            {
                AntiCheatService.Punish(person, "inventory stack too large");
                return;
            }
        }

        if (hasSnapshot && !AreChangesAuthorized(person, currentCounts))
        {
            AntiCheatService.Punish(person, "unauthorized inventory mutation");
            Snapshot(currentCounts);
            return;
        }

        Snapshot(currentCounts);
    }

    private bool AreChangesAuthorized(PersonComponent person, Dictionary<string, int> currentCounts)
    {
        InventoryMutationTracker tracker = person.GetComponent<InventoryMutationTracker>();
        foreach (KeyValuePair<string, int> pair in currentCounts)
        {
            lastCounts.TryGetValue(pair.Key, out int previous);
            int delta = pair.Value - previous;
            if (delta != 0 && (tracker == null || !tracker.ConsumeAllowedChange(pair.Key, delta)))
            {
                return false;
            }
        }

        foreach (KeyValuePair<string, int> pair in lastCounts)
        {
            if (currentCounts.ContainsKey(pair.Key))
            {
                continue;
            }

            int delta = -pair.Value;
            if (delta != 0 && (tracker == null || !tracker.ConsumeAllowedChange(pair.Key, delta)))
            {
                return false;
            }
        }

        return true;
    }

    private void Snapshot(Dictionary<string, int> currentCounts)
    {
        lastCounts.Clear();
        foreach (KeyValuePair<string, int> pair in currentCounts)
        {
            lastCounts[pair.Key] = pair.Value;
        }

        hasSnapshot = true;
    }
}
