using System.Collections.Generic;
using Unity.AI.Navigation;
using UnityEngine;

public static class ResourceRuntimeBootstrap
{
    private const string WorldGenerationSeedKey = "DBD.WorldGenerationSeed";
    private const float LandformXSpread = 1f;
    private const float LandformZSpread = 1f;
    private static readonly Vector2 SpawnXRange = new(-900f, 900f);
    private static readonly Vector2 SpawnZRange = new(-900f, 900f);
    private static readonly HashSet<Vector2Int> GeneratedResourceChunks = new();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateResourcesIfMissing()
    {
        BranchResource[] existingResources = Object.FindObjectsByType<BranchResource>(FindObjectsSortMode.None);
        bool worldAlreadyReady = existingResources.Length > 12
            && GameObject.Find("Tree") != null
            && GameObject.Find("Workbench") != null
            && GameObject.Find("River") != null
            && GameObject.Find("Mountain") != null
            && GameObject.Find("High Mountain") != null
            && GameObject.Find("Waterfall") != null
            && GameObject.Find("Canyon") != null;

        if (worldAlreadyReady)
        {
            PaintTerrainZones();
            CreateTerrainLandforms();
            CreateWaterLandforms();
            EnsureResourceChunksAround(new Vector3(0f, 0f, 8f), WorldChunkService.InitialChunkRadius);
            foreach (BranchResource resource in existingResources)
            {
                EnsureResourceCollider(resource.gameObject, Vector3.one * 1.8f);
            }

            EnsureStarterResourcesNearSpawn();
            return;
        }

        Random.State previousRandomState = Random.state;
        Random.InitState(GetOrCreateWorldGenerationSeed());

        EnvironmentRuntimeBootstrap.EnsureEnvironment();
        PaintTerrainZones();
        CreateTerrainLandforms();
        CreateRiver();
        CreateWaterLandforms();
        CreateTrees();
        CreateLooseTreeDrops();
        EnsureResourceChunksAround(new Vector3(0f, 0f, 8f), WorldChunkService.InitialChunkRadius);
        CreateStarterResourcesNearSpawn();
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
            if (IsGeneratedTerrainZone(oldZone.name))
            {
                Object.Destroy(oldZone);
            }
        }

        CreateTerrainZone("Meadow", new Vector3(-16f, 0.012f, 16f), new Vector3(24f, 0.04f, 15f), new Color(0.30f, 0.56f, 0.24f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Dirt Zone", new Vector3(17f, 0.014f, -2f), new Vector3(18f, 0.04f, 14f), new Color(0.27f, 0.34f, 0.18f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Clay Bank", new Vector3(-26f, 0.016f, -10f), new Vector3(13f, 0.04f, 8f), new Color(0.44f, 0.30f, 0.22f, 1f), PrimitiveType.Cube);

        for (int i = 0; i < 5; i++)
        {
            CreateTerrainZone(
                "Sand Zone",
                new Vector3(-31f + i * 14f, 0.018f, -15f + Mathf.Sin(i * 1.4f) * 3f),
                new Vector3(Random.Range(8f, 13f), 0.04f, Random.Range(4f, 7f)),
                new Color(0.78f, 0.58f, 0.25f, 1f),
                PrimitiveType.Cube);
        }
    }

    private static bool IsGeneratedTerrainZone(string zoneName)
    {
        return zoneName == "Dirt Zone"
            || zoneName == "Sand Zone"
            || zoneName == "Meadow"
            || zoneName == "Clay Bank"
            || zoneName == "Mountain"
            || zoneName == "High Mountain"
            || zoneName == "Great Mountain"
            || zoneName == "Hill"
            || zoneName == "Plateau"
            || zoneName == "Valley"
            || zoneName == "Rocky Ridge"
            || zoneName == "Canyon"
            || zoneName == "Cliff"
            || zoneName == "Basin"
            || zoneName == "Cave Mouth"
            || zoneName == "Cave"
            || zoneName == "Limestone Cave"
            || zoneName == "Tunnel"
            || zoneName == "Crater"
            || zoneName == "Pond"
            || zoneName == "Lake"
            || zoneName == "Wetland"
            || zoneName == "Marsh"
            || zoneName == "Wide River"
            || zoneName == "Narrow River"
            || zoneName == "Waterfall";
    }

    private static void CreateTerrainLandforms()
    {
        for (int i = 0; i < 7; i++)
        {
            CreateTerrainZone(
                "Mountain",
                new Vector3(18f + Mathf.Sin(i * 0.8f) * 4f, 0.28f, -20f + i * 8f),
                new Vector3(Random.Range(6f, 10f), Random.Range(1.2f, 2.4f), Random.Range(5f, 8f)),
                new Color(0.33f, 0.36f, 0.34f, 1f),
                PrimitiveType.Sphere);
        }

        for (int i = 0; i < 4; i++)
        {
            CreateTerrainZone(
                "High Mountain",
                new Vector3(31f + Mathf.Sin(i * 1.1f) * 3f, 0.8f, -18f + i * 13f),
                new Vector3(Random.Range(7f, 11f), Random.Range(2.8f, 4.4f), Random.Range(6f, 10f)),
                new Color(0.28f, 0.31f, 0.30f, 1f),
                PrimitiveType.Sphere);
        }

        for (int i = 0; i < 3; i++)
        {
            CreateTerrainZone(
                "Great Mountain",
                new Vector3(-28f + i * 9f, 0.55f, 25f - Mathf.Sin(i) * 4f),
                new Vector3(Random.Range(9f, 13f), Random.Range(2.2f, 3.3f), Random.Range(8f, 12f)),
                new Color(0.36f, 0.38f, 0.35f, 1f),
                PrimitiveType.Sphere);
        }

        for (int i = 0; i < 6; i++)
        {
            CreateTerrainZone(
                "Hill",
                new Vector3(-35f + i * 13f, 0.16f, 12f + Mathf.Sin(i * 0.9f) * 6f),
                new Vector3(Random.Range(5f, 8f), Random.Range(0.5f, 1f), Random.Range(5f, 8f)),
                new Color(0.32f, 0.50f, 0.24f, 1f),
                PrimitiveType.Sphere);
        }

        CreateTerrainZone("Plateau", new Vector3(3f, 0.22f, -21f), new Vector3(18f, 0.5f, 8f), new Color(0.39f, 0.42f, 0.30f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Valley", new Vector3(-6f, 0.035f, -8f), new Vector3(22f, 0.05f, 6f), new Color(0.23f, 0.42f, 0.20f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Basin", new Vector3(-21f, 0.04f, 4f), new Vector3(14f, 0.06f, 10f), new Color(0.29f, 0.39f, 0.22f, 1f), PrimitiveType.Sphere);
        CreateTerrainZone("Crater", new Vector3(26f, 0.045f, 16f), new Vector3(10f, 0.07f, 10f), new Color(0.26f, 0.24f, 0.21f, 1f), PrimitiveType.Sphere);
        CreateTerrainZone("Cave Mouth", new Vector3(32f, 0.42f, 8f), new Vector3(5f, 2f, 3f), new Color(0.10f, 0.10f, 0.10f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Cave", new Vector3(29f, 0.12f, 12f), new Vector3(9f, 0.25f, 6f), new Color(0.08f, 0.08f, 0.09f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Limestone Cave", new Vector3(-32f, 0.14f, -2f), new Vector3(8f, 0.28f, 5f), new Color(0.58f, 0.56f, 0.50f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Tunnel", new Vector3(4f, 0.10f, 25f), new Vector3(15f, 0.2f, 2.5f), new Color(0.13f, 0.12f, 0.11f, 1f), PrimitiveType.Cube);

        for (int i = 0; i < 6; i++)
        {
            CreateTerrainZone(
                "Rocky Ridge",
                new Vector3(-6f + i * 5.5f, 0.16f, 20f + Mathf.Sin(i) * 2.5f),
                new Vector3(Random.Range(4f, 7f), Random.Range(0.6f, 1.2f), Random.Range(3f, 5f)),
                new Color(0.43f, 0.42f, 0.39f, 1f),
                PrimitiveType.Cube);
        }

        for (int i = 0; i < 8; i++)
        {
            CreateTerrainZone(
                "Canyon",
                new Vector3(-34f + i * 8.8f, 0.08f, 2f + Mathf.Sin(i * 0.7f) * 4f),
                new Vector3(Random.Range(6f, 9f), 0.28f, Random.Range(2.8f, 4.2f)),
                new Color(0.50f, 0.26f, 0.17f, 1f),
                PrimitiveType.Cube);
        }

        for (int i = 0; i < 5; i++)
        {
            CreateTerrainZone(
                "Cliff",
                new Vector3(-36f + i * 18f, 0.5f, -24f + Mathf.Sin(i * 1.7f) * 3f),
                new Vector3(Random.Range(4f, 7f), Random.Range(1.8f, 3.2f), Random.Range(2f, 4f)),
                new Color(0.30f, 0.30f, 0.29f, 1f),
                PrimitiveType.Cube);
        }
    }

    private static void CreateWaterLandforms()
    {
        for (int i = 0; i < 4; i++)
        {
            GameObject wideRiver = CreateTerrainZone(
                "Wide River",
                new Vector3(-34f + i * 22f, 0.10f, -2f + Mathf.Sin(i * 0.8f) * 4f),
                new Vector3(11f, 0.11f, 4.8f),
                new Color(0.10f, 0.32f, 0.78f, 0.9f),
                PrimitiveType.Sphere);
            wideRiver.transform.rotation = Quaternion.Euler(0f, i * 12f, 0f);
        }

        for (int i = 0; i < 8; i++)
        {
            GameObject narrowRiver = CreateTerrainZone(
                "Narrow River",
                new Vector3(-36f + i * 9.5f, 0.09f, 20f + Mathf.Sin(i * 0.75f) * 4f),
                new Vector3(5.8f, 0.09f, 1.15f),
                new Color(0.16f, 0.45f, 0.95f, 0.9f),
                PrimitiveType.Sphere);
            narrowRiver.transform.rotation = Quaternion.Euler(0f, -18f + i * 7f, 0f);
        }

        CreateTerrainZone("Waterfall", new Vector3(30f, 1.05f, -4f), new Vector3(2.5f, 3.4f, 0.55f), new Color(0.70f, 0.88f, 1f, 0.9f), PrimitiveType.Cube);
        CreateTerrainZone("Pond", new Vector3(-18f, 0.075f, 20f), new Vector3(7f, 0.08f, 4.8f), new Color(0.09f, 0.38f, 0.62f, 0.9f), PrimitiveType.Sphere);
        CreateTerrainZone("Lake", new Vector3(9f, 0.07f, 23f), new Vector3(12f, 0.08f, 7f), new Color(0.08f, 0.34f, 0.72f, 0.9f), PrimitiveType.Sphere);
        CreateTerrainZone("Wetland", new Vector3(-30f, 0.045f, 11f), new Vector3(11f, 0.05f, 7f), new Color(0.13f, 0.31f, 0.20f, 1f), PrimitiveType.Cube);
        CreateTerrainZone("Marsh", new Vector3(23f, 0.045f, -12f), new Vector3(10f, 0.05f, 6f), new Color(0.16f, 0.28f, 0.16f, 1f), PrimitiveType.Cube);
    }

    private static GameObject CreateTerrainZone(string name, Vector3 position, Vector3 scale, Color color, PrimitiveType primitive)
    {
        bool visibleFeature = name == "Cave Mouth" || name == "Cave" || name == "Limestone Cave" || name == "Tunnel";
        GameObject zone = visibleFeature ? GameObject.CreatePrimitive(primitive) : new GameObject(name);
        zone.name = name;
        Vector3 spreadPosition = GetSeededLandformPosition(name, position);
        zone.transform.position = new Vector3(spreadPosition.x * LandformXSpread, spreadPosition.y, spreadPosition.z * LandformZSpread);
        zone.transform.localScale = scale;
        if (zone.TryGetComponent(out Renderer renderer))
        {
            renderer.material.color = color;
        }

        zone.AddComponent<TerrainZoneInfo>().Configure(name, IsWalkableTerrain(name), GetTerrainSpeedMultiplier(name));
        Collider collider = zone.GetComponent<Collider>();
        if (collider != null)
        {
            Object.Destroy(collider);
        }

        return zone;
    }

    private static bool IsWalkableTerrain(string zoneName)
    {
        return zoneName != "River"
            && zoneName != "Wide River"
            && zoneName != "Narrow River"
            && zoneName != "Waterfall"
            && zoneName != "Pond"
            && zoneName != "Lake"
            && zoneName != "Mountain"
            && zoneName != "High Mountain"
            && zoneName != "Great Mountain"
            && zoneName != "Cliff";
    }

    private static float GetTerrainSpeedMultiplier(string zoneName)
    {
        switch (zoneName)
        {
            case "Meadow":
            case "Valley":
                return 1.1f;
            case "Dirt Zone":
            case "Clay Bank":
            case "Plateau":
            case "Basin":
                return 0.9f;
            case "Sand Zone":
            case "Hill":
            case "Canyon":
            case "Crater":
                return 0.75f;
            case "Wetland":
                return 0.55f;
            case "Marsh":
                return 0.45f;
            case "Rocky Ridge":
            case "Cave":
            case "Limestone Cave":
            case "Tunnel":
            case "Cave Mouth":
                return 0.65f;
            default:
                return 1f;
        }
    }

    private static void CreateRiver()
    {
        for (int i = 0; i < 12; i++)
        {
            GameObject riverPiece = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            riverPiece.name = "River";
            riverPiece.transform.position = GetRiverPosition(i / 11f);
            riverPiece.transform.localScale = new Vector3(8.2f, 0.12f, 2.8f + Mathf.Sin(i * 0.8f) * 0.8f);
            riverPiece.GetComponent<Renderer>().material.color = new Color(0.12f, 0.35f, 0.90f, 0.85f);
            riverPiece.AddComponent<TerrainZoneInfo>().Configure("River", false, 1f);
            Object.Destroy(riverPiece.GetComponent<Collider>());
        }
    }

    private static void CreateTrees()
    {
        for (int i = 0; i < 12; i++)
        {
            Vector3 position = Random.value < 0.75f ? GetRandomPosition("Meadow") : GetRandomPosition();
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
        SpawnSeries("leaf", "Leaf", 6, 4, 0.1f, new Color(0.18f, 0.62f, 0.16f, 1f), PrimitiveType.Sphere, new Vector3(0.45f, 0.08f, 0.30f), "Meadow", "Wetland", "Marsh", "Valley");
        SpawnSeries("branch", "Branch", 5, 4, 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.12f, 0.18f), "Meadow", "Hill", "Valley");
        SpawnSeries("wood", "Wood", 6, 3, 10f, new Color(0.36f, 0.19f, 0.08f, 1f), PrimitiveType.Cube, new Vector3(0.8f, 0.45f, 0.45f), "Meadow", "Hill");
        SpawnSeries("sand", "Sand", 3, 5, 1.2f, new Color(0.78f, 0.58f, 0.25f, 1f), PrimitiveType.Sphere, new Vector3(0.65f, 0.18f, 0.65f), "Sand Zone", "Wide River", "Lake");
        SpawnSeries("stone", "Stone", 5, 5, 30f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.45f, 0.65f), "Mountain", "High Mountain", "Great Mountain", "Rocky Ridge", "Canyon", "Cliff", "Crater");
        SpawnSeries("dirt", "Dirt", 4, 4, 1.5f, new Color(0.24f, 0.48f, 0.20f, 1f), PrimitiveType.Sphere, new Vector3(0.65f, 0.18f, 0.65f), "Dirt Zone", "Clay Bank", "Basin", "Valley");
        SpawnSeries("coal", "Coal", 3, 4, 18f, new Color(0.05f, 0.05f, 0.05f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), "Mountain", "High Mountain", "Canyon", "Cave Mouth", "Cave", "Tunnel");
        SpawnSeries("copper", "Copper", 3, 4, 24f, new Color(0.75f, 0.34f, 0.14f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), "Rocky Ridge", "Canyon", "Plateau");
        SpawnResource("lead", "Lead", 4, 26f, new Color(0.28f, 0.30f, 0.35f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), "High Mountain", "Canyon", "Cave Mouth", "Cave");
        SpawnSeries("tin", "Tin", 3, 4, 22f, new Color(0.70f, 0.72f, 0.70f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), "Rocky Ridge", "Mountain", "Plateau");
        SpawnSeries("iron", "Iron", 3, 5, 30f, new Color(0.50f, 0.45f, 0.40f, 1f), PrimitiveType.Cube, new Vector3(0.7f, 0.5f, 0.7f), "High Mountain", "Great Mountain", "Mountain");
        SpawnSeries("water", "Water", 3, 4, 0.8f, new Color(0.12f, 0.35f, 0.90f, 1f), PrimitiveType.Sphere, new Vector3(0.55f, 0.12f, 0.55f), "River", "Wide River", "Narrow River", "Waterfall", "Pond", "Lake", "Wetland");
        SpawnSeries("flint", "Flint", 3, 5, 8f, new Color(0.18f, 0.19f, 0.20f, 1f), PrimitiveType.Cube, new Vector3(0.45f, 0.25f, 0.35f), "Canyon", "Rocky Ridge", "Cliff", "Crater", "Limestone Cave", "Tunnel");
    }

    public static void EnsureResourceChunksAround(Vector3 worldPosition, int radius)
    {
        Vector2Int center = WorldChunkService.GetChunkCoord(worldPosition);
        for (int z = -radius; z <= radius; z++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                EnsureResourceChunk(new Vector2Int(center.x + x, center.y + z));
            }
        }
    }

    private static void EnsureResourceChunk(Vector2Int chunkCoord)
    {
        if (GeneratedResourceChunks.Contains(chunkCoord) || GameObject.Find(GetChunkRootName(chunkCoord)) != null)
        {
            GeneratedResourceChunks.Add(chunkCoord);
            return;
        }

        Random.State previousRandomState = Random.state;
        Random.InitState(WorldChunkService.GetChunkSeed(chunkCoord));

        GameObject chunkRoot = new GameObject(GetChunkRootName(chunkCoord));
        Vector3 center = WorldChunkService.GetChunkCenter(chunkCoord);
        WorldSample centerSample = WorldChunkService.Sample(center);
        int resourceCount = GetChunkResourceCount(centerSample);
        for (int i = 0; i < resourceCount; i++)
        {
            Vector3 position = GetRandomPositionInChunk(chunkCoord);
            WorldSample sample = WorldChunkService.Sample(position);
            if (!sample.Walkable)
            {
                continue;
            }

            SpawnChunkResource(sample, position, i, chunkRoot.transform);
        }

        GeneratedResourceChunks.Add(chunkCoord);
        Random.state = previousRandomState;
    }

    private static int GetChunkResourceCount(WorldSample sample)
    {
        switch (sample.Biome)
        {
            case WorldBiome.Mountain:
            case WorldBiome.Canyon:
            case WorldBiome.CaveRegion:
                return Random.Range(7, 12);
            case WorldBiome.Wetland:
            case WorldBiome.RiverBank:
                return Random.Range(5, 9);
            case WorldBiome.Hill:
                return Random.Range(6, 10);
            default:
                return Random.Range(4, 8);
        }
    }

    private static Vector3 GetRandomPositionInChunk(Vector2Int chunkCoord)
    {
        Vector3 center = WorldChunkService.GetChunkCenter(chunkCoord);
        float half = WorldChunkService.ChunkSize * 0.5f - 8f;
        Vector3 position = new Vector3(
            center.x + Random.Range(-half, half),
            0f,
            center.z + Random.Range(-half, half));
        return PlaceOnTerrain(position);
    }

    private static void SpawnChunkResource(WorldSample sample, Vector3 position, int index, Transform parent)
    {
        switch (sample.Biome)
        {
            case WorldBiome.Mountain:
                SpawnWeightedChunkResource(position, parent, index, 0.45f, "stone", "Stone", 5, 24f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.45f, 0.65f), "iron", "Iron", 3, 30f, new Color(0.50f, 0.45f, 0.40f, 1f));
                break;
            case WorldBiome.Canyon:
            case WorldBiome.CaveRegion:
                SpawnWeightedChunkResource(position, parent, index, 0.38f, "coal", "Coal", 3, 18f, new Color(0.05f, 0.05f, 0.05f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), "copper", "Copper", 3, 24f, new Color(0.75f, 0.34f, 0.14f, 1f));
                break;
            case WorldBiome.Wetland:
                SpawnWeightedChunkResource(position, parent, index, 0.55f, "leaf", "Leaf", 6, 0.1f, new Color(0.18f, 0.62f, 0.16f, 1f), PrimitiveType.Sphere, new Vector3(0.45f, 0.08f, 0.30f), "water", "Water", 3, 0.8f, new Color(0.12f, 0.35f, 0.90f, 1f));
                break;
            case WorldBiome.RiverBank:
                SpawnWeightedChunkResource(position, parent, index, 0.55f, "sand", "Sand", 3, 1.2f, new Color(0.78f, 0.58f, 0.25f, 1f), PrimitiveType.Sphere, new Vector3(0.65f, 0.18f, 0.65f), "flint", "Flint", 3, 8f, new Color(0.18f, 0.19f, 0.20f, 1f));
                break;
            case WorldBiome.Hill:
                SpawnWeightedChunkResource(position, parent, index, 0.48f, "stone", "Stone", 5, 24f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.45f, 0.65f), "wood", "Wood", 6, 10f, new Color(0.36f, 0.19f, 0.08f, 1f));
                break;
            default:
                SpawnWeightedChunkResource(position, parent, index, 0.45f, "branch", "Branch", 5, 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.12f, 0.18f), "wood", "Wood", 6, 10f, new Color(0.36f, 0.19f, 0.08f, 1f));
                break;
        }
    }

    private static void SpawnWeightedChunkResource(
        Vector3 position,
        Transform parent,
        int index,
        float firstChance,
        string firstPrefix,
        string firstName,
        int firstVariants,
        float firstDuration,
        Color firstColor,
        PrimitiveType primitive,
        Vector3 scale,
        string secondPrefix,
        string secondName,
        int secondVariants,
        float secondDuration,
        Color secondColor)
    {
        bool first = Random.value < firstChance;
        string prefix = first ? firstPrefix : secondPrefix;
        string displayName = first ? firstName : secondName;
        int variants = first ? firstVariants : secondVariants;
        float duration = first ? firstDuration : secondDuration;
        Color color = first ? firstColor : secondColor;
        int variant = Random.Range(1, variants + 1);
        SpawnChunkResourceObject(prefix + "_" + variant, displayName + " " + variant, position, duration, color, primitive, scale * Random.Range(1.0f, 1.8f), parent, index);
    }

    private static void SpawnChunkResourceObject(string itemId, string displayName, Vector3 position, float duration, Color color, PrimitiveType primitive, Vector3 scale, Transform parent, int index)
    {
        GameObject resourceObject = GameObject.CreatePrimitive(primitive);
        resourceObject.name = displayName;
        resourceObject.transform.SetParent(parent, true);
        resourceObject.transform.position = position;
        resourceObject.transform.localScale = scale;
        resourceObject.GetComponent<Renderer>().material.color = color;
        BranchResource resource = resourceObject.AddComponent<BranchResource>();
        resource.Configure(itemId, displayName, duration);
        EnsureResourceCollider(resourceObject, Vector3.one * 1.6f);
        AddNavIgnore(resourceObject);
    }

    private static string GetChunkRootName(Vector2Int chunkCoord)
    {
        return $"Resource Chunk {chunkCoord.x},{chunkCoord.y}";
    }

    private static void CreateWorkbench()
    {
        GameObject workbench = GameObject.CreatePrimitive(PrimitiveType.Cube);
        workbench.name = "Workbench";
        workbench.transform.position = PlaceOnTerrain(new Vector3(-4f, 0f, 0f)) + Vector3.up * 0.45f;
        workbench.transform.localScale = new Vector3(2.2f, 0.9f, 1.2f);
        workbench.GetComponent<Renderer>().material.color = new Color(0.43f, 0.25f, 0.12f, 1f);
        workbench.AddComponent<WorkbenchCraftingStation>();
        AddNavIgnore(workbench);
    }

    private static void SpawnSeries(string idPrefix, string namePrefix, int variants, int countPerVariant, float baseDuration, Color color, PrimitiveType primitive, Vector3 baseScale, params string[] zoneNames)
    {
        for (int variant = 1; variant <= variants; variant++)
        {
            SpawnResource(idPrefix + "_" + variant, namePrefix + " " + variant, countPerVariant, baseDuration, color, primitive, baseScale, zoneNames);
        }
    }

    private static void SpawnResource(string itemId, string displayName, int count, float baseDuration, Color color, PrimitiveType primitive, Vector3 baseScale, params string[] zoneNames)
    {
        for (int i = 0; i < count; i++)
        {
            float size = Random.Range(0.65f, 1.8f);
            GameObject resourceObject = GameObject.CreatePrimitive(primitive);
            resourceObject.name = displayName;
            resourceObject.transform.position = zoneNames != null && zoneNames.Length > 0 ? GetRandomPosition(zoneNames) : GetRandomPosition();
            resourceObject.transform.localScale = baseScale * size * 1.35f;
            resourceObject.GetComponent<Renderer>().material.color = color;
            BranchResource resource = resourceObject.AddComponent<BranchResource>();
            resource.Configure(itemId, displayName, baseDuration * size);
            EnsureResourceCollider(resourceObject, Vector3.one * 1.6f);
            AddNavIgnore(resourceObject);
        }
    }

    private static void EnsureStarterResourcesNearSpawn()
    {
        BranchResource[] resources = Object.FindObjectsByType<BranchResource>(FindObjectsSortMode.None);
        int nearbyCount = 0;
        Vector3 spawnCenter = new Vector3(0f, 0f, 8f);
        for (int i = 0; i < resources.Length; i++)
        {
            Vector3 resourcePosition = resources[i].transform.position;
            if (Vector2.Distance(new Vector2(resourcePosition.x, resourcePosition.z), new Vector2(spawnCenter.x, spawnCenter.z)) <= 80f)
            {
                nearbyCount++;
            }
        }

        if (nearbyCount >= 14)
        {
            return;
        }

        CreateStarterResourcesNearSpawn();
    }

    private static void CreateStarterResourcesNearSpawn()
    {
        SpawnStarterResource("wood_1", "Starter Wood", new Vector3(-14f, 0f, 18f), 5f, new Color(0.36f, 0.19f, 0.08f, 1f), PrimitiveType.Cube, new Vector3(1.2f, 0.6f, 0.6f));
        SpawnStarterResource("wood_2", "Starter Wood", new Vector3(-9f, 0f, 23f), 5f, new Color(0.34f, 0.17f, 0.07f, 1f), PrimitiveType.Cube, new Vector3(1.1f, 0.55f, 0.55f));
        SpawnStarterResource("branch_1", "Starter Branch", new Vector3(-4f, 0f, 15f), 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(1.05f, 0.22f, 0.28f));
        SpawnStarterResource("branch_2", "Starter Branch", new Vector3(4f, 0f, 18f), 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(1.05f, 0.22f, 0.28f));
        SpawnStarterResource("leaf_1", "Starter Leaf", new Vector3(9f, 0f, 14f), 0.1f, new Color(0.18f, 0.62f, 0.16f, 1f), PrimitiveType.Sphere, new Vector3(0.75f, 0.16f, 0.55f));
        SpawnStarterResource("stone_1", "Starter Stone", new Vector3(14f, 0f, 20f), 18f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.9f, 0.65f, 0.8f));
        SpawnStarterResource("stone_2", "Starter Stone", new Vector3(18f, 0f, 12f), 18f, new Color(0.38f, 0.39f, 0.41f, 1f), PrimitiveType.Cube, new Vector3(0.85f, 0.6f, 0.75f));
        SpawnStarterResource("flint_1", "Starter Flint", new Vector3(22f, 0f, 22f), 8f, new Color(0.18f, 0.19f, 0.20f, 1f), PrimitiveType.Cube, new Vector3(0.6f, 0.35f, 0.5f));
        SpawnStarterResource("dirt_1", "Starter Dirt", new Vector3(-18f, 0f, 10f), 1.2f, new Color(0.28f, 0.36f, 0.18f, 1f), PrimitiveType.Sphere, new Vector3(0.8f, 0.28f, 0.8f));
        SpawnStarterResource("sand_1", "Starter Sand", new Vector3(12f, 0f, 29f), 1.2f, new Color(0.78f, 0.58f, 0.25f, 1f), PrimitiveType.Sphere, new Vector3(0.8f, 0.25f, 0.8f));
    }

    private static void SpawnStarterResource(string itemId, string displayName, Vector3 position, float baseDuration, Color color, PrimitiveType primitive, Vector3 scale)
    {
        GameObject resourceObject = GameObject.CreatePrimitive(primitive);
        resourceObject.name = displayName;
        resourceObject.transform.position = PlaceOnTerrain(position);
        resourceObject.transform.localScale = scale;
        resourceObject.GetComponent<Renderer>().material.color = color;
        BranchResource resource = resourceObject.AddComponent<BranchResource>();
        resource.Configure(itemId, displayName, baseDuration);
        EnsureResourceCollider(resourceObject, Vector3.one * 1.8f);
        AddNavIgnore(resourceObject);
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
        return PlaceOnTerrain(new Vector3(Random.Range(SpawnXRange.x, SpawnXRange.y), 0f, Random.Range(SpawnZRange.x, SpawnZRange.y)));
    }

    private static Vector3 GetRandomPosition(params string[] zoneNames)
    {
        if (zoneNames == null || zoneNames.Length == 0)
        {
            return GetRandomPosition();
        }

        for (int attempt = 0; attempt < 80; attempt++)
        {
            string zoneName = zoneNames[Random.Range(0, zoneNames.Length)];
            Vector3 candidate;
            if (zoneName == "River" || zoneName == "Wide River" || zoneName == "Narrow River")
            {
                candidate = GetRiverPosition(Random.value);
                candidate.x += Random.Range(-18f, 18f);
                candidate.z += Random.Range(-24f, 24f);
            }
            else
            {
                candidate = new Vector3(Random.Range(SpawnXRange.x, SpawnXRange.y), 0f, Random.Range(SpawnZRange.x, SpawnZRange.y));
            }

            candidate = PlaceOnTerrain(candidate);
            if (MatchesTerrainRequest(candidate, zoneName))
            {
                return candidate;
            }
        }

        return GetRandomPosition();
    }

    private static Vector3 GetRiverPosition(float normalized)
    {
        float x = Mathf.Lerp(SpawnXRange.x + 4f, SpawnXRange.y - 4f, normalized);
        float z = Mathf.Sin(normalized * Mathf.PI * 3.2f) * 170f + Mathf.Cos(normalized * Mathf.PI * 1.4f) * 70f;
        return PlaceOnTerrain(new Vector3(x, 0f, z));
    }

    private static int GetOrCreateWorldGenerationSeed()
    {
        if (PlayerPrefs.HasKey(WorldGenerationSeedKey))
        {
            return PlayerPrefs.GetInt(WorldGenerationSeedKey);
        }

        int seed = unchecked((int)System.DateTime.UtcNow.Ticks ^ Random.Range(int.MinValue, int.MaxValue));
        PlayerPrefs.SetInt(WorldGenerationSeedKey, seed);
        PlayerPrefs.Save();
        return seed;
    }

    private static Vector3 GetSeededLandformPosition(string zoneName, Vector3 position)
    {
        if (zoneName == "Waterfall" || zoneName == "Cave Mouth")
        {
            return position;
        }

        float noiseX = Mathf.PerlinNoise((position.x + 100f) * 0.08f, (position.z + 100f) * 0.08f);
        float noiseZ = Mathf.PerlinNoise((position.x - 30f) * 0.08f, (position.z + 40f) * 0.08f);
        float jitterAmount = zoneName == "River" || zoneName == "Wide River" || zoneName == "Narrow River" ? 1.8f : 4.5f;
        return new Vector3(
            position.x + (noiseX - 0.5f) * jitterAmount,
            position.y,
            position.z + (noiseZ - 0.5f) * jitterAmount);
    }

    private static Vector3 PlaceOnTerrain(Vector3 position)
    {
        position.y = EnvironmentRuntimeBootstrap.GetTerrainHeight(position) + 0.18f;
        return position;
    }

    private static bool MatchesTerrainRequest(Vector3 position, string zoneName)
    {
        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return true;
        }

        Vector2 normalized = EnvironmentRuntimeBootstrap.WorldToNormalized(position);
        float height01 = terrain.terrainData.GetInterpolatedHeight(normalized.x, normalized.y) / EnvironmentRuntimeBootstrap.TerrainHeight;
        float steepness = terrain.terrainData.GetSteepness(normalized.x, normalized.y);
        bool water = EnvironmentRuntimeBootstrap.IsWaterAt(position);
        float moisture = Mathf.PerlinNoise(normalized.x * 9.4f + 8f, normalized.y * 9.4f + 31f);

        switch (zoneName)
        {
            case "River":
            case "Wide River":
            case "Narrow River":
            case "Waterfall":
            case "Pond":
            case "Lake":
                return !water && height01 < 0.18f && moisture > 0.35f;
            case "Wetland":
            case "Marsh":
                return !water && moisture > 0.58f && height01 < 0.22f;
            case "Mountain":
            case "High Mountain":
            case "Great Mountain":
                return !water && height01 > 0.22f;
            case "Rocky Ridge":
            case "Cliff":
            case "Canyon":
            case "Crater":
            case "Cave":
            case "Cave Mouth":
            case "Limestone Cave":
            case "Tunnel":
                return !water && (steepness > 14f || height01 > 0.18f);
            case "Sand Zone":
                return !water && height01 < 0.18f;
            case "Meadow":
            case "Valley":
            case "Hill":
                return !water && height01 < 0.34f && moisture < 0.84f;
            default:
                return !water;
        }
    }
}
