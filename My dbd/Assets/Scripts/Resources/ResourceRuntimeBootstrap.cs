using System.Collections.Generic;
using Unity.AI.Navigation;
using UnityEngine;

public static class ResourceRuntimeBootstrap
{
    private const int WorldGenerationSeed = 43117;
    private static readonly Vector2 SpawnXRange = new(-38f, 38f);
    private static readonly Vector2 SpawnZRange = new(-22f, 30f);

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateResourcesIfMissing()
    {
        BranchResource[] existingResources = Object.FindObjectsByType<BranchResource>(FindObjectsSortMode.None);
        bool worldAlreadyReady = existingResources.Length > 12
            && GameObject.Find("Tree") != null
            && GameObject.Find("Workbench") != null
            && GameObject.Find("River") != null;

        if (worldAlreadyReady)
        {
            PaintTerrainZones();
            foreach (BranchResource resource in existingResources)
            {
                EnsureResourceCollider(resource.gameObject, Vector3.one * 1.8f);
            }

            return;
        }

        Random.State previousRandomState = Random.state;
        Random.InitState(WorldGenerationSeed);

        EnvironmentRuntimeBootstrap.EnsureEnvironment();
        PaintTerrainZones();
        CreateRiver();
        CreateTrees();
        CreateLooseTreeDrops();
        CreateBasicResources();
        CreateWorkbench();

        Random.state = previousRandomState;
    }

    private static void PaintTerrainZones()
    {
        GameObject ground = GameObject.Find("Ground");
        if (ground != null && ground.TryGetComponent(out Renderer groundRenderer))
        {
            groundRenderer.material.color = new Color(0.24f, 0.48f, 0.20f, 1f);
            if (ground.GetComponent<TerrainSurfaceEnforcer>() == null)
            {
                ground.AddComponent<TerrainSurfaceEnforcer>();
            }
        }

        foreach (GameObject oldZone in GameObject.FindGameObjectsWithTag("Untagged"))
        {
            if (oldZone.name == "Dirt Zone" || oldZone.name == "Sand Zone")
            {
                Object.Destroy(oldZone);
            }
        }

        for (int i = 0; i < 4; i++)
        {
            GameObject sand = GameObject.CreatePrimitive(PrimitiveType.Cube);
            sand.name = "Sand Zone";
            sand.transform.position = new Vector3(Random.Range(-34f, 34f), 0.01f, Random.Range(-18f, 26f));
            sand.transform.localScale = new Vector3(Random.Range(7f, 10f), 0.04f, Random.Range(4f, 6f));
            sand.GetComponent<Renderer>().material.color = new Color(0.78f, 0.58f, 0.25f, 1f);
            Object.Destroy(sand.GetComponent<Collider>());
        }
    }

    private static void CreateRiver()
    {
        for (int i = 0; i < 7; i++)
        {
            GameObject riverPiece = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            riverPiece.name = "River";
            riverPiece.transform.position = new Vector3(-32f + (i * 10.5f), 0.08f, -8f + Mathf.Sin(i * 0.9f) * 5f);
            riverPiece.transform.localScale = new Vector3(8.5f, 0.12f, 2.6f);
            riverPiece.GetComponent<Renderer>().material.color = new Color(0.12f, 0.35f, 0.90f, 0.85f);
            Object.Destroy(riverPiece.GetComponent<Collider>());
        }
    }

    private static void CreateTrees()
    {
        for (int i = 0; i < 12; i++)
        {
            Vector3 position = GetRandomPosition();
            float size = Random.Range(0.85f, 1.75f);

            GameObject treeRoot = new GameObject("Tree");
            treeRoot.transform.position = position;
            treeRoot.transform.localScale = Vector3.one * size;

            GameObject trunk = GameObject.CreatePrimitive(PrimitiveType.Cube);
            trunk.name = "Tree Trunk";
            trunk.transform.SetParent(treeRoot.transform, false);
            trunk.transform.localPosition = new Vector3(0f, 1.2f, 0f);
            trunk.transform.localScale = new Vector3(0.55f, 2.4f, 0.55f);
            trunk.GetComponent<Renderer>().material.color = new Color(0.36f, 0.20f, 0.09f, 1f);

            GameObject leaves = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            leaves.name = "Tree Crown";
            leaves.transform.SetParent(treeRoot.transform, false);
            leaves.transform.localPosition = new Vector3(0f, 2.8f, 0f);
            leaves.transform.localScale = new Vector3(2.2f, 1.3f, 2.2f);
            leaves.GetComponent<Renderer>().material.color = new Color(0.12f, 0.46f, 0.13f, 1f);
            Object.Destroy(leaves.GetComponent<Collider>());

            BoxCollider collider = treeRoot.AddComponent<BoxCollider>();
            collider.center = new Vector3(0f, 1.4f, 0f);
            collider.size = new Vector3(2.2f, 3f, 2.2f);

            BranchResource resource = treeRoot.AddComponent<BranchResource>();
            resource.ConfigureYields("Tree", 10f * size, new[]
            {
                new PersonInventoryItem("leaf_" + Random.Range(1, 7), Random.Range(4, 9)),
                new PersonInventoryItem("branch_" + Random.Range(1, 6), Random.Range(2, 5)),
                new PersonInventoryItem("wood_" + Random.Range(1, 7), Random.Range(1, 3))
            });

            TreeDropMaintainer maintainer = treeRoot.AddComponent<TreeDropMaintainer>();
            maintainer.Configure(10f);
            AddNavIgnore(treeRoot);
        }
    }

    private static void CreateLooseTreeDrops()
    {
        foreach (GameObject tree in GameObject.FindGameObjectsWithTag("Untagged"))
        {
            if (tree.name != "Tree")
            {
                continue;
            }

            TreeDropMaintainer maintainer = tree.GetComponent<TreeDropMaintainer>();
            if (maintainer != null)
            {
                maintainer.RefillNow();
            }
        }
    }

    private static void CreateBasicResources()
    {
        SpawnSeries("leaf", "Leaf", 6, 3, 0.1f, new Color(0.18f, 0.62f, 0.16f, 1f), PrimitiveType.Sphere, new Vector3(0.45f, 0.08f, 0.30f));
        SpawnSeries("branch", "Branch", 5, 3, 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.12f, 0.18f));
        SpawnSeries("wood", "Wood", 6, 2, 10f, new Color(0.36f, 0.19f, 0.08f, 1f), PrimitiveType.Cube, new Vector3(0.8f, 0.45f, 0.45f));
        SpawnSeries("sand", "Sand", 3, 3, 1.2f, new Color(0.78f, 0.58f, 0.25f, 1f), PrimitiveType.Sphere, new Vector3(0.65f, 0.18f, 0.65f));
        SpawnSeries("stone", "Stone", 5, 3, 30f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.45f, 0.65f));
        SpawnSeries("dirt", "Dirt", 4, 3, 1.5f, new Color(0.24f, 0.48f, 0.20f, 1f), PrimitiveType.Sphere, new Vector3(0.65f, 0.18f, 0.65f));
        SpawnSeries("coal", "Coal", 3, 3, 18f, new Color(0.05f, 0.05f, 0.05f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f));
        SpawnSeries("copper", "Copper", 3, 3, 24f, new Color(0.75f, 0.34f, 0.14f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f));
        SpawnResource("lead", "Lead", 3, 26f, new Color(0.28f, 0.30f, 0.35f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f));
        SpawnSeries("tin", "Tin", 3, 3, 22f, new Color(0.70f, 0.72f, 0.70f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f));
        SpawnSeries("iron", "Iron", 3, 3, 30f, new Color(0.50f, 0.45f, 0.40f, 1f), PrimitiveType.Cube, new Vector3(0.7f, 0.5f, 0.7f));
        SpawnSeries("water", "Water", 3, 3, 0.8f, new Color(0.12f, 0.35f, 0.90f, 1f), PrimitiveType.Sphere, new Vector3(0.55f, 0.12f, 0.55f));
        SpawnSeries("flint", "Flint", 3, 3, 8f, new Color(0.18f, 0.19f, 0.20f, 1f), PrimitiveType.Cube, new Vector3(0.45f, 0.25f, 0.35f));
    }

    private static void CreateWorkbench()
    {
        GameObject workbench = GameObject.CreatePrimitive(PrimitiveType.Cube);
        workbench.name = "Workbench";
        workbench.transform.position = new Vector3(-4f, 0.45f, 0f);
        workbench.transform.localScale = new Vector3(2.2f, 0.9f, 1.2f);
        workbench.GetComponent<Renderer>().material.color = new Color(0.43f, 0.25f, 0.12f, 1f);
        workbench.AddComponent<WorkbenchCraftingStation>();
        AddNavIgnore(workbench);
    }

    private static void SpawnSeries(string idPrefix, string namePrefix, int variants, int countPerVariant, float baseDuration, Color color, PrimitiveType primitive, Vector3 baseScale)
    {
        for (int variant = 1; variant <= variants; variant++)
        {
            SpawnResource(idPrefix + "_" + variant, namePrefix + " " + variant, countPerVariant, baseDuration, color, primitive, baseScale);
        }
    }

    private static void SpawnResource(string itemId, string displayName, int count, float baseDuration, Color color, PrimitiveType primitive, Vector3 baseScale)
    {
        for (int i = 0; i < count; i++)
        {
            float size = Random.Range(0.65f, 1.8f);
            GameObject resourceObject = GameObject.CreatePrimitive(primitive);
            resourceObject.name = displayName;
            resourceObject.transform.position = GetRandomPosition();
            resourceObject.transform.localScale = baseScale * size;
            resourceObject.GetComponent<Renderer>().material.color = color;
            BranchResource resource = resourceObject.AddComponent<BranchResource>();
            resource.Configure(itemId, displayName, baseDuration * size);
            EnsureResourceCollider(resourceObject, Vector3.one * 1.6f);
            AddNavIgnore(resourceObject);
        }
    }

    public static GameObject CreateTreeDrop(string itemId, string displayName, Vector3 position)
    {
        float size = Random.Range(0.65f, 1.4f);
        bool isLeaf = itemId.StartsWith("leaf");
        GameObject drop = GameObject.CreatePrimitive(isLeaf ? PrimitiveType.Sphere : PrimitiveType.Cube);
        drop.name = displayName;
        drop.transform.position = position;
        drop.transform.localScale = (isLeaf ? new Vector3(0.45f, 0.08f, 0.30f) : new Vector3(0.75f, 0.12f, 0.18f)) * size;
        drop.GetComponent<Renderer>().material.color = isLeaf
            ? new Color(0.18f, 0.62f, 0.16f, 1f)
            : new Color(0.45f, 0.25f, 0.10f, 1f);
        BranchResource resource = drop.AddComponent<BranchResource>();
        resource.Configure(itemId, displayName, 0.1f * size);
        EnsureResourceCollider(drop, Vector3.one * 1.2f);
        AddNavIgnore(drop);
        return drop;
    }

    private static void EnsureResourceCollider(GameObject resource, Vector3 fallbackSize)
    {
        Collider collider = resource.GetComponent<Collider>();
        if (collider == null)
        {
            BoxCollider box = resource.AddComponent<BoxCollider>();
            box.size = fallbackSize;
        }
    }

    private static void AddNavIgnore(GameObject target)
    {
        NavMeshModifier modifier = target.GetComponent<NavMeshModifier>();
        if (modifier == null)
        {
            modifier = target.AddComponent<NavMeshModifier>();
        }

        modifier.ignoreFromBuild = true;
    }

    private static Vector3 GetRandomPosition()
    {
        return new Vector3(Random.Range(SpawnXRange.x, SpawnXRange.y), 0.18f, Random.Range(SpawnZRange.x, SpawnZRange.y));
    }
}
