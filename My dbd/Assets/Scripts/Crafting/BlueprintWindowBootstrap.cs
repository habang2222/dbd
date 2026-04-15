using UnityEngine;

namespace Dbd.Crafting
{
    public static class BlueprintWindowBootstrap
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void CreateBlueprintWindow()
        {
            if (Object.FindAnyObjectByType<BlueprintWindowController>() != null)
            {
                return;
            }

            GameObject window = new GameObject("Blueprint Window");
            window.AddComponent<BlueprintWindowController>();
        }
    }
}
