using UnityEngine;

#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
#endif

public static class SmartChaseSceneSetup
{
#if UNITY_EDITOR
    [MenuItem("Tools/Setup Smart Chase Simulation")]
    public static void Setup()
    {
        GameObject simulationObject = GameObject.Find("Smart Chase Simulation");
        if (simulationObject == null)
        {
            simulationObject = new GameObject("Smart Chase Simulation");
        }

        if (simulationObject.GetComponent<SmartChaseSimulation>() == null)
        {
            simulationObject.AddComponent<SmartChaseSimulation>();
        }

        Camera camera = Camera.main;
        if (camera != null)
        {
            camera.transform.position = new Vector3(0f, 0f, -10f);
            camera.orthographic = true;
            camera.orthographicSize = 5.8f;
        }

        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene());
    }
#endif
}
