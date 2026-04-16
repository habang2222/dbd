using UnityEngine;

public enum SessionRole
{
    Player,
    Director
}

public static class SessionRoleService
{
    public const string PlayerClientId = "local_player";
    public const string DirectorClientId = "local_director";
    public const string DirectorControlledClientId = "director_controlled";
    private const string RoleKey = "DBD.SessionRole";

    public static SessionRole CurrentRole { get; private set; } = SessionRole.Director;
    public static bool IsDirector => CurrentRole == SessionRole.Director;
    public static bool IsPlayer => CurrentRole == SessionRole.Player;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void LoadRole()
    {
        CurrentRole = PlayerPrefs.GetInt(RoleKey, 1) == 1 ? SessionRole.Director : SessionRole.Player;
    }

    public static void SetRole(SessionRole role)
    {
        CurrentRole = role;
        PlayerPrefs.SetInt(RoleKey, role == SessionRole.Director ? 1 : 0);
        PlayerPrefs.Save();
        ApplyDefaultOwnership();
        RefreshRoleAwareUi();
    }

    public static bool CanControl(PersonComponent person)
    {
        if (person == null)
        {
            return false;
        }

        return IsDirector || person.OwnerClientId == PlayerClientId;
    }

    public static string GetRoleName()
    {
        return IsDirector ? "Director" : "Player";
    }

    public static void ApplyDefaultOwnership()
    {
        PersonComponent[] people = Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None);
        System.Array.Sort(people, (left, right) => string.Compare(left.PersonId, right.PersonId, System.StringComparison.Ordinal));

        for (int i = 0; i < people.Length; i++)
        {
            PersonComponent person = people[i];
            if (person == null)
            {
                continue;
            }

            person.SetTeam(GameAuthority.LocalTeamId);
            person.SetOwnerClient(i == 0 ? PlayerClientId : DirectorControlledClientId);
            if (!CanControl(person) && person.IsSelected)
            {
                person.SetSelected(false);
            }
        }
    }

    public static void RefreshRoleAwareUi()
    {
        SessionRoleHud hud = Object.FindFirstObjectByType<SessionRoleHud>();
        if (hud != null)
        {
            hud.Refresh();
        }

        DirectorToolHud directorHud = Object.FindFirstObjectByType<DirectorToolHud>();
        if (directorHud != null)
        {
            directorHud.Refresh();
        }

        if (MainGameTopBar.Instance != null)
        {
            MainGameTopBar.Instance.RefreshForRole();
        }

        if (!IsDirector)
        {
            if (DirectorCreationWindow.Instance != null)
            {
                DirectorCreationWindow.Instance.Hide();
            }

            if (DirectorTerrainWindow.Instance != null)
            {
                DirectorTerrainWindow.Instance.Hide();
            }

            if (DirectorIntentBrushSystem.Instance != null)
            {
                DirectorIntentBrushSystem.Instance.Cancel();
            }
        }

        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }

        if (ActionWindow.Instance != null)
        {
            ActionWindow.Instance.RefreshForSelectedPerson();
        }
    }
}
