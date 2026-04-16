using UnityEngine;

public enum MultiplayerVisibility
{
    SharedWorld,
    LocalOnly
}

public static class GameAuthority
{
    public const string LocalClientId = "local_player";
    public const string LocalTeamId = "local_team";
    public const bool OfflineLocalMode = true;

    public static bool IsServerAuthority => true;

    public static bool CanIssueCommand(PersonComponent person)
    {
        return string.IsNullOrEmpty(GetCommandRejectReason(person));
    }

    public static string GetCommandRejectReason(PersonComponent person)
    {
        if (person == null)
        {
            return "missing unit";
        }

        if (person.Stats == null)
        {
            return "missing unit stats";
        }

        if (person.Stats.health <= 0f)
        {
            return "dead unit command";
        }

        if (SessionRoleService.IsPlayer && !ServerWorldStateService.IsOpenToPlayers)
        {
            return "world is still in director setup";
        }

        if (!SessionRoleService.CanControl(person))
        {
            return "ownership violation";
        }

        return string.Empty;
    }

    public static bool IsOwnedByLocalClient(PersonComponent person)
    {
        return SessionRoleService.CanControl(person);
    }

    public static void EnsureLocalOwnership(PersonComponent person)
    {
        if (person == null)
        {
            return;
        }

        if (string.IsNullOrWhiteSpace(person.OwnerClientId))
        {
            person.SetOwnerClient(SessionRoleService.PlayerClientId);
        }

        if (string.IsNullOrWhiteSpace(person.TeamId))
        {
            person.SetTeam(LocalTeamId);
        }
    }

    public static MultiplayerVisibility GetVisibilityForObject(Object target)
    {
        if (target is Component component)
        {
            if (component.GetComponentInParent<Canvas>() != null)
            {
                return MultiplayerVisibility.LocalOnly;
            }
        }

        return MultiplayerVisibility.SharedWorld;
    }
}
