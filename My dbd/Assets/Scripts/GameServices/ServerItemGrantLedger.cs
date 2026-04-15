using System.Collections.Generic;
using UnityEngine;

public static class ServerItemGrantLedger
{
    private static readonly Dictionary<PersonComponent, Dictionary<string, int>> grants = new();

    public static bool IsServerItemGrantActive { get; private set; }

    public static bool TryBeginGrant(PersonComponent owner, string itemId, int count)
    {
        if (!GameAuthority.IsServerAuthority || owner == null || string.IsNullOrWhiteSpace(itemId) || count <= 0)
        {
            return false;
        }

        IsServerItemGrantActive = true;
        Dictionary<string, int> ownerGrants = GetOwnerGrants(owner);
        ownerGrants.TryGetValue(itemId, out int current);
        ownerGrants[itemId] = current + count;
        return true;
    }

    public static void EndGrant()
    {
        IsServerItemGrantActive = false;
    }

    public static void CancelGrant(PersonComponent owner, string itemId, int count)
    {
        if (owner == null || string.IsNullOrWhiteSpace(itemId) || count <= 0)
        {
            return;
        }

        if (!grants.TryGetValue(owner, out Dictionary<string, int> ownerGrants)
            || !ownerGrants.TryGetValue(itemId, out int available))
        {
            return;
        }

        available = Mathf.Max(0, available - count);
        if (available == 0)
        {
            ownerGrants.Remove(itemId);
        }
        else
        {
            ownerGrants[itemId] = available;
        }
    }

    public static bool ConsumeGrant(PersonComponent owner, string itemId, int count)
    {
        if (owner == null || string.IsNullOrWhiteSpace(itemId) || count <= 0)
        {
            return false;
        }

        if (!grants.TryGetValue(owner, out Dictionary<string, int> ownerGrants)
            || !ownerGrants.TryGetValue(itemId, out int available)
            || available < count)
        {
            return false;
        }

        available -= count;
        if (available == 0)
        {
            ownerGrants.Remove(itemId);
        }
        else
        {
            ownerGrants[itemId] = available;
        }

        return true;
    }

    private static Dictionary<string, int> GetOwnerGrants(PersonComponent owner)
    {
        if (!grants.TryGetValue(owner, out Dictionary<string, int> ownerGrants))
        {
            ownerGrants = new Dictionary<string, int>();
            grants[owner] = ownerGrants;
        }

        return ownerGrants;
    }
}
