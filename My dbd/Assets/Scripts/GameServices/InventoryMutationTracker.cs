using System.Collections.Generic;
using UnityEngine;

public class InventoryMutationTracker : MonoBehaviour
{
    private readonly Dictionary<string, int> allowedDeltas = new();

    public void AllowChange(string itemId, int delta)
    {
        if (string.IsNullOrWhiteSpace(itemId) || delta == 0)
        {
            return;
        }

        allowedDeltas.TryGetValue(itemId, out int current);
        allowedDeltas[itemId] = current + delta;
    }

    public bool ConsumeAllowedChange(string itemId, int delta)
    {
        if (delta == 0)
        {
            return true;
        }

        if (string.IsNullOrWhiteSpace(itemId) || !allowedDeltas.TryGetValue(itemId, out int allowed))
        {
            return false;
        }

        if ((delta > 0 && allowed < delta) || (delta < 0 && allowed > delta))
        {
            return false;
        }

        allowed -= delta;
        if (allowed == 0)
        {
            allowedDeltas.Remove(itemId);
        }
        else
        {
            allowedDeltas[itemId] = allowed;
        }

        return true;
    }
}
