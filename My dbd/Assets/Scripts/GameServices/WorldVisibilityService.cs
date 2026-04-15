using UnityEngine;

public static class WorldVisibilityService
{
    private const float EnemyMapRevealDistance = 28f;

    public static bool CanLocalPlayerSeePerson(PersonComponent person)
    {
        if (person == null)
        {
            return false;
        }

        string localTeam = GetLocalTeamId();
        return !string.IsNullOrWhiteSpace(localTeam) && person.TeamId == localTeam;
    }

    public static bool CanLocalPlayerSeeEnemy(EnemyComponent enemy)
    {
        if (enemy == null)
        {
            return false;
        }

        foreach (PersonComponent person in Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (!CanLocalPlayerSeePerson(person))
            {
                continue;
            }

            if (Vector3.Distance(person.transform.position, enemy.transform.position) <= EnemyMapRevealDistance)
            {
                return true;
            }
        }

        return false;
    }

    private static string GetLocalTeamId()
    {
        foreach (PersonComponent person in Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (person.IsSelected)
            {
                return person.TeamId;
            }
        }

        if (GameAuthority.OfflineLocalMode)
        {
            return GameAuthority.LocalTeamId;
        }

        return string.Empty;
    }
}
