using Unity.AI.Navigation;
using UnityEngine;

public static class EnemyRuntimeBootstrap
{
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateEnemiesIfMissing()
    {
        if (PlayerPrefs.GetInt(ResourceRuntimeBootstrap.WorldClearedKey, 0) == 1)
        {
            return;
        }

        EnvironmentRuntimeBootstrap.EnsureEnvironment();

        EnemyComponent[] existingEnemies = Object.FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None);
        if (existingEnemies.Length > 0)
        {
            EnsureExistingEnemiesCanAct(existingEnemies);
            return;
        }

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
            7f + (index * 3f),
            100f);

        EnemyWanderer wanderer = enemyObject.AddComponent<EnemyWanderer>();
        wanderer.Initialize(enemyObject.transform.position, 8f);

        enemyObject.AddComponent<UnitCombatController>();
        enemyObject.AddComponent<UnitDeathShrink>();

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

    private static void EnsureExistingEnemiesCanAct(EnemyComponent[] enemies)
    {
        foreach (EnemyComponent enemy in enemies)
        {
            if (enemy == null)
            {
                continue;
            }

            EnsureEnemyCollision(enemy.gameObject);

            if (enemy.Stats.stamina <= 0f)
            {
                enemy.Stats.stamina = 100f;
            }

            if (enemy.GetComponent<EnemyWanderer>() == null)
            {
                EnemyWanderer wanderer = enemy.gameObject.AddComponent<EnemyWanderer>();
                wanderer.Initialize(enemy.transform.position, 8f);
            }

            if (enemy.GetComponent<UnitCombatController>() == null)
            {
                enemy.gameObject.AddComponent<UnitCombatController>();
            }

            if (enemy.GetComponent<UnitDeathShrink>() == null)
            {
                enemy.gameObject.AddComponent<UnitDeathShrink>();
            }
        }
    }

    private static void EnsureEnemyCollision(GameObject enemyObject)
    {
        Collider collider = enemyObject.GetComponent<Collider>();
        if (collider == null)
        {
            collider = enemyObject.AddComponent<SphereCollider>();
        }

        collider.isTrigger = false;

        NavMeshModifier modifier = enemyObject.GetComponent<NavMeshModifier>();
        if (modifier == null)
        {
            modifier = enemyObject.AddComponent<NavMeshModifier>();
        }

        modifier.ignoreFromBuild = true;
    }
}
