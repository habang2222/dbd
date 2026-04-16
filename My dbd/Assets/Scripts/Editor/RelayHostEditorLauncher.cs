using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class RelayHostEditorLauncher
{
    public static void StartRelayHost()
    {
        EditorSceneManager.OpenScene("Assets/Scenes/SampleScene.unity");
        EditorApplication.EnterPlaymode();
        Debug.Log("Relay host play mode requested.");
    }
}
