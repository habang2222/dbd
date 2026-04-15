using UnityEngine;

public static class RuntimeUiRepairBootstrap
{
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void RepairRuntimeUi()
    {
        EnsureMainGameTopBar();
        EnsureBlueprintWindow();
        EnsureWindowMenu();
        EnsureUnitList();
        EnsureMapWindow();
    }

    private static void EnsureMainGameTopBar()
    {
        if (MainGameTopBar.Instance != null || Object.FindFirstObjectByType<MainGameTopBar>() != null)
        {
            return;
        }

        GameObject topBar = new GameObject("Main Game Top Bar");
        topBar.AddComponent<MainGameTopBar>();
    }

    private static void EnsureBlueprintWindow()
    {
        if (Dbd.Crafting.BlueprintWindowController.Instance != null || Object.FindFirstObjectByType<Dbd.Crafting.BlueprintWindowController>() != null)
        {
            return;
        }

        GameObject window = new GameObject("Blueprint Window");
        window.AddComponent<Dbd.Crafting.BlueprintWindowController>();
    }

    private static void EnsureWindowMenu()
    {
        if (WindowMenuPanel.Instance != null || Object.FindFirstObjectByType<WindowMenuPanel>() != null)
        {
            return;
        }

        GameObject panel = new GameObject("Window Menu Panel");
        panel.AddComponent<WindowMenuPanel>();
    }

    private static void EnsureUnitList()
    {
        if (UnitListPanel.Instance != null || Object.FindFirstObjectByType<UnitListPanel>() != null)
        {
            return;
        }

        GameObject panel = new GameObject("Unit List Panel");
        panel.AddComponent<UnitListPanel>();
    }

    private static void EnsureMapWindow()
    {
        if (MapWindow.Instance != null || Object.FindFirstObjectByType<MapWindow>() != null)
        {
            return;
        }

        GameObject window = new GameObject("Map Window");
        window.AddComponent<MapWindow>();
    }
}
