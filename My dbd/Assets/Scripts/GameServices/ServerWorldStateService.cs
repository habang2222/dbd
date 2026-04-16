using UnityEngine;

public enum ServerWorldState
{
    DirectorSetup,
    OpenToPlayers
}

public static class ServerWorldStateService
{
    private const string StateKey = "DBD.ServerWorldState";

    public static ServerWorldState CurrentState { get; private set; } = ServerWorldState.DirectorSetup;
    public static bool IsDirectorSetup => CurrentState == ServerWorldState.DirectorSetup;
    public static bool IsOpenToPlayers => CurrentState == ServerWorldState.OpenToPlayers;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void LoadState()
    {
        CurrentState = PlayerPrefs.GetInt(StateKey, 0) == 1
            ? ServerWorldState.OpenToPlayers
            : ServerWorldState.DirectorSetup;
    }

    public static bool CanPlayerAccessWorld()
    {
        return IsOpenToPlayers || SessionRoleService.IsDirector;
    }

    public static void OpenToPlayers()
    {
        SetState(ServerWorldState.OpenToPlayers);
    }

    public static void ReturnToSetup()
    {
        SetState(ServerWorldState.DirectorSetup);
    }

    public static string GetStateName()
    {
        return IsOpenToPlayers ? "Open" : "Director Setup";
    }

    private static void SetState(ServerWorldState state)
    {
        CurrentState = state;
        PlayerPrefs.SetInt(StateKey, state == ServerWorldState.OpenToPlayers ? 1 : 0);
        PlayerPrefs.Save();
        ServerBackupService.RequestImmediateBackup("server_state_changed_" + GetStateName());
        SessionRoleService.RefreshRoleAwareUi();
    }
}
