using Unity.AI.Navigation;
using UnityEngine;

public static class EnemyRuntimeBootstrap
{
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateEnemiesIfMissing()
    {
        if (Object.FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None).Length > 0)
        {
            return;
        }

        EnvironmentRuntimeBootstrap.EnsureEnvironment();

        for (int i = 0; i < 3; i++)
        {
            CreateEnemy(i);
        }
    }

    private static void CreateEnemy(int index)
    {
        GameObject enemyObject = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        enemyObject.transform.position = new Vector3(14f + (index * 4f), 0.75f, 8f + (index * 3f));
        enemyObject.transform.localScale = new Vector3(1.5f, 1.5f, 1.5f);

        EnemyComponent enemy = enemyObject.AddComponent<EnemyComponent>();
        enemy.Initialize(
            $"enemy_{index + 1}",
            $"Enemy_{index + 1}",
            70f + (index * 20f),
            7f + (index * 3f));

        Renderer renderer = enemyObject.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.material.color = new Color(0.85f, 0.12f, 0.12f, 1f);
        }

        Collider collider = enemyObject.GetComponent<Collider>();
        if (collider != null)
        {
            collider.isTrigger = false;
        }

        NavMeshModifier modifier = enemyObject.AddComponent<NavMeshModifier>();
        modifier.ignoreFromBuild = true;
    }
}
