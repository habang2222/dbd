using UnityEngine;
using UnityEngine.AI;

public static class AntiCheatService
{
    private const float FreezeSeconds = 180f;
    private const float MaxCommandDistance = 2400f;
    private const float NavMeshSampleRadius = 5f;
    private const int MaxSingleInventoryChange = 500;

    public static bool CanAcceptMoveCommand(PersonComponent person, Vector3 destination, out string reason)
    {
        reason = string.Empty;
        if (!GameAuthority.CanIssueCommand(person))
        {
            reason = GameAuthority.GetCommandRejectReason(person);
            if (person != null && !GameAuthority.IsOwnedByLocalClient(person))
            {
                Punish(person, reason);
            }

            return false;
        }

        if (IsFrozen(person))
        {
            reason = "unit is frozen";
            return false;
        }

        if (!IsFinite(destination))
        {
            reason = "invalid destination";
            Punish(person, reason);
            return false;
        }

        float distance = Vector3.Distance(person.transform.position, destination);
        if (distance > MaxCommandDistance)
        {
            reason = "move command too far";
            return false;
        }

        if (!NavMesh.SamplePosition(destination, out _, NavMeshSampleRadius, NavMesh.AllAreas))
        {
            reason = "destination outside navmesh";
            return false;
        }

        if (!TerrainZoneInfo.CanStandAt(destination))
        {
            reason = "destination blocked by terrain";
            return false;
        }

        return true;
    }

    public static bool IsFrozen(PersonComponent person)
    {
        AntiCheatFreezeLock freezeLock = person != null ? person.GetComponent<AntiCheatFreezeLock>() : null;
        return freezeLock != null && freezeLock.IsFrozen;
    }

    public static void Punish(PersonComponent person, string reason)
    {
        if (person == null)
        {
            return;
        }

        AntiCheatAuditLog.Record(person, reason);
        AntiCheatFreezeLock freezeLock = person.GetComponent<AntiCheatFreezeLock>();
        if (freezeLock == null)
        {
            freezeLock = person.gameObject.AddComponent<AntiCheatFreezeLock>();
        }

        freezeLock.Freeze(FreezeSeconds, reason);
    }

    public static bool CanAcceptInventoryAdd(PersonComponent person, string itemId, int count, out string reason)
    {
        reason = string.Empty;
        if (!GameAuthority.CanIssueCommand(person))
        {
            reason = GameAuthority.GetCommandRejectReason(person);
            if (person != null && !GameAuthority.IsOwnedByLocalClient(person))
            {
                Punish(person, reason);
            }

            return false;
        }

        if (IsFrozen(person))
        {
            reason = "unit is frozen";
            return false;
        }

        if (string.IsNullOrWhiteSpace(itemId) || count <= 0 || count > MaxSingleInventoryChange)
        {
            reason = "invalid inventory add";
            Punish(person, reason);
            return false;
        }

        return true;
    }

    public static bool CanAcceptInventoryRemove(PersonComponent person, string itemId, int count, out string reason)
    {
        reason = string.Empty;
        if (!GameAuthority.CanIssueCommand(person))
        {
            reason = GameAuthority.GetCommandRejectReason(person);
            if (person != null && !GameAuthority.IsOwnedByLocalClient(person))
            {
                Punish(person, reason);
            }

            return false;
        }

        if (IsFrozen(person))
        {
            reason = "unit is frozen";
            return false;
        }

        if (string.IsNullOrWhiteSpace(itemId) || count <= 0 || count > MaxSingleInventoryChange)
        {
            reason = "invalid inventory remove";
            Punish(person, reason);
            return false;
        }

        if (!InventoryService.CanRemoveItem(person, itemId, count))
        {
            reason = "tried to spend missing item";
            Punish(person, reason);
            return false;
        }

        return true;
    }

    public static bool IsFinite(Vector3 value)
    {
        return float.IsFinite(value.x) && float.IsFinite(value.y) && float.IsFinite(value.z);
    }
}
