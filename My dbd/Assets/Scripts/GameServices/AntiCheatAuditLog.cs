using System.Collections.Generic;
using UnityEngine;

public static class AntiCheatAuditLog
{
    private const int MaxEntries = 80;
    private static readonly Queue<string> entries = new();

    public static void Record(PersonComponent person, string reason)
    {
        string owner = person != null ? person.OwnerClientId : "unknown_owner";
        string unit = person != null ? person.PersonName : "unknown_unit";
        string entry = $"[{Time.time:0.00}] {unit} owner={owner}: {reason}";
        entries.Enqueue(entry);
        while (entries.Count > MaxEntries)
        {
            entries.Dequeue();
        }

        Debug.LogWarning("ANTI-CHEAT: " + entry);
    }

    public static IEnumerable<string> Entries => entries;
}
