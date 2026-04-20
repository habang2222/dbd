using System.Collections.Generic;
using Unity.AI.Navigation;
using UnityEngine;

public enum DirectorZoneType
{
    Meadow,
    Forest,
    Mountain,
    Canyon,
    River,
    Wetland
}

public sealed class DirectorPaintZone
{
    public readonly DirectorZoneType Type;
    public readonly Vector3 Center;
    public readonly float Radius;
    public readonly float Intensity;

    public DirectorPaintZone(DirectorZoneType type, Vector3 center, float radius, float intensity)
    {
        Type = type;
        Center = center;
        Radius = radius;
        Intensity = intensity;
    }
}

public static class DirectorWorldPlanningService
{
    private const string MarkerRootName = "Director Painted Zones";
    private const string AppliedRootName = "Director Applied Details";
    private static readonly List<DirectorPaintZone> PendingZones = new();

    public static int PendingZoneCount => PendingZones.Count;

    public static void PaintZone(DirectorZoneType type, Vector3 center, float radius, float intensity)
    {
        if (type != DirectorZoneType.Meadow && type != DirectorZoneType.Forest)
        {
            return;
        }

        center = PlaceOnTerrain(center);
        PendingZones.Add(new DirectorPaintZone(type, center, Mathf.Max(8f, radius), Mathf.Clamp01(intensity)));
        CreatePaintMarker(type, center, radius);
        RefreshDirectorTerrainWindow();
    }

    public static void ApplyPendingZones()
    {
        if (PendingZones.Count == 0)
        {
            return;
        }

        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            PendingZones.Clear();
            ClearPaintMarkers();
            RefreshDirectorTerrainWindow();
            return;
        }

        Random.State previousRandomState = Random.state;
        Random.InitState(PendingZones.Count * 73856093 ^ Mathf.RoundToInt(Time.realtimeSinceStartup * 1000f));

        for (int i = 0; i < PendingZones.Count; i++)
        {
            ApplyZoneToTerrain(terrain, PendingZones[i]);
        }

        for (int i = 0; i < PendingZones.Count; i++)
        {
            ApplyZoneDetails(PendingZones[i]);
        }

        Random.state = previousRandomState;
        PendingZones.Clear();
        ClearPaintMarkers();
        RefreshDirectorTerrainWindow();
    }

    public static void FinishWorldSetup()
    {
        ApplyPendingZones();
        ServerWorldStateService.OpenToPlayers();
    }

    public static void ClearPendingZones()
    {
        PendingZones.Clear();
        ClearPaintMarkers();
        RefreshDirectorTerrainWindow();
    }

    private static void ApplyZoneToTerrain(Terrain terrain, DirectorPaintZone zone)
    {
        TerrainData data = terrain.terrainData;
        Vector3 local = zone.Center - terrain.transform.position;
        int centerX = Mathf.RoundToInt(local.x / data.size.x * (data.heightmapResolution - 1));
        int centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.heightmapResolution - 1));
        int radiusSamples = Mathf.Max(2, Mathf.RoundToInt(zone.Radius / data.size.x * data.heightmapResolution));
        int startX = Mathf.Clamp(centerX - radiusSamples, 0, data.heightmapResolution - 1);
        int startZ = Mathf.Clamp(centerZ - radiusSamples, 0, data.heightmapResolution - 1);
        int width = Mathf.Clamp(centerX + radiusSamples, 0, data.heightmapResolution - 1) - startX + 1;
        int height = Mathf.Clamp(centerZ + radiusSamples, 0, data.heightmapResolution - 1) - startZ + 1;
        float[,] heights = data.GetHeights(startX, startZ, width, height);
        float centerHeight = data.GetHeight(centerX, centerZ) / data.size.y;

        for (int z = 0; z < height; z++)
        {
            for (int x = 0; x < width; x++)
            {
                float dx = (startX + x - centerX) / (float)radiusSamples;
                float dz = (startZ + z - centerZ) / (float)radiusSamples;
                float distance = Mathf.Sqrt(dx * dx + dz * dz);
                if (distance > 1f)
                {
                    continue;
                }

                float falloff = Mathf.SmoothStep(1f, 0f, distance);
                float noise = Mathf.PerlinNoise((startX + x) * 0.08f, (startZ + z) * 0.08f);
                heights[z, x] = GetProcessedHeight(zone, heights[z, x], centerHeight, falloff, noise, distance, data.size.y);
            }
        }

        data.SetHeights(startX, startZ, heights);
    }

    private static float GetProcessedHeight(DirectorPaintZone zone, float current, float centerHeight, float falloff, float noise, float distance, float terrainHeight)
    {
        float strength = zone.Intensity;
        switch (zone.Type)
        {
            case DirectorZoneType.Meadow:
                return Mathf.Lerp(current, centerHeight - 1.5f / terrainHeight, falloff * 0.70f * strength);
            case DirectorZoneType.Forest:
                return Mathf.Lerp(current, centerHeight + (noise - 0.5f) * 5f / terrainHeight, falloff * 0.45f * strength);
            case DirectorZoneType.Mountain:
            {
                float peak = Mathf.Pow(falloff, 1.45f) * (0.7f + noise * 0.55f);
                return Mathf.Clamp01(current + peak * 72f / terrainHeight * strength);
            }
            case DirectorZoneType.Canyon:
            {
                float trench = Mathf.Pow(falloff, 0.72f) * (0.85f + noise * 0.25f);
                float rim = Mathf.Clamp01(1f - Mathf.Abs(distance - 0.74f) / 0.15f) * 16f / terrainHeight;
                return Mathf.Clamp01(current - trench * 48f / terrainHeight * strength + rim * strength);
            }
            case DirectorZoneType.River:
            {
                float channel = Mathf.Pow(falloff, 0.45f);
                return Mathf.Lerp(current, centerHeight - 7f / terrainHeight, channel * 0.85f * strength);
            }
            case DirectorZoneType.Wetland:
                return Mathf.Lerp(current, centerHeight - 3f / terrainHeight, falloff * 0.55f * strength);
            default:
                return current;
        }
    }

    private static void ApplyZoneDetails(DirectorPaintZone zone)
    {
        if (ResourceRuntimeBootstrap.IsWorldCleared)
        {
            return;
        }

        Transform root = GetOrCreateAppliedRoot().transform;
        switch (zone.Type)
        {
            case DirectorZoneType.Meadow:
                SpawnGrassPatches(zone, root, 14);
                SpawnLooseResource(zone, root, "branch_1", "Meadow Branch", 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.12f, 0.18f), 5);
                break;
            case DirectorZoneType.Forest:
                SpawnGrassPatches(zone, root, 10);
                SpawnTrees(zone, root, Mathf.RoundToInt(Mathf.Clamp(zone.Radius / 12f, 4f, 16f)));
                break;
            case DirectorZoneType.Mountain:
                SpawnLooseResource(zone, root, "stone_1", "Mountain Stone", 18f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.9f, 0.62f, 0.8f), 8);
                SpawnLooseResource(zone, root, "iron_1", "Mountain Iron", 30f, new Color(0.50f, 0.45f, 0.40f, 1f), PrimitiveType.Cube, new Vector3(0.7f, 0.5f, 0.7f), 3);
                break;
            case DirectorZoneType.Canyon:
                SpawnLooseResource(zone, root, "coal_1", "Canyon Coal", 18f, new Color(0.05f, 0.05f, 0.05f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), 5);
                SpawnLooseResource(zone, root, "copper_1", "Canyon Copper", 24f, new Color(0.75f, 0.34f, 0.14f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.45f, 0.65f), 3);
                break;
            case DirectorZoneType.River:
                SpawnWaterPatch(zone, root);
                SpawnLooseResource(zone, root, "sand_1", "River Sand", 1.2f, new Color(0.78f, 0.58f, 0.25f, 1f), PrimitiveType.Sphere, new Vector3(0.7f, 0.2f, 0.7f), 7);
                break;
            case DirectorZoneType.Wetland:
                SpawnWaterPatch(zone, root);
                SpawnGrassPatches(zone, root, 18);
                SpawnLooseResource(zone, root, "leaf_1", "Wetland Leaf", 0.1f, new Color(0.18f, 0.62f, 0.16f, 1f), PrimitiveType.Sphere, new Vector3(0.55f, 0.12f, 0.42f), 6);
                break;
        }
    }

    private static void SpawnTrees(DirectorPaintZone zone, Transform root, int count)
    {
        for (int i = 0; i < count; i++)
        {
            Vector3 position = GetRandomPointInZone(zone);
            GameObject treeRoot = new GameObject("Director Forest Tree");
            treeRoot.transform.SetParent(root, true);
            treeRoot.transform.position = position;
            float size = Random.Range(0.85f, 1.7f);
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
            resource.ConfigureYields("Forest Tree", 10f * size, new[]
            {
                new PersonInventoryItem("leaf_1", Random.Range(4, 9)),
                new PersonInventoryItem("branch_1", Random.Range(2, 5)),
                new PersonInventoryItem("wood_1", Random.Range(1, 3))
            });
            AddNavIgnore(treeRoot);
        }
    }

    private static void SpawnGrassPatches(DirectorPaintZone zone, Transform root, int count)
    {
        for (int i = 0; i < count; i++)
        {
            GameObject grass = GameObject.CreatePrimitive(PrimitiveType.Cube);
            grass.name = "Director Grass Patch";
            grass.transform.SetParent(root, true);
            grass.transform.position = GetRandomPointInZone(zone) + Vector3.up * 0.08f;
            float size = Random.Range(0.7f, 1.8f);
            grass.transform.localScale = new Vector3(size, 0.12f, size * Random.Range(0.6f, 1.4f));
            grass.transform.rotation = Quaternion.Euler(0f, Random.Range(0f, 360f), 0f);
            grass.GetComponent<Renderer>().material.color = new Color(0.18f, 0.52f, 0.18f, 1f);
            Object.Destroy(grass.GetComponent<Collider>());
        }
    }

    private static void SpawnLooseResource(DirectorPaintZone zone, Transform root, string itemId, string displayName, float duration, Color color, PrimitiveType primitive, Vector3 scale, int count)
    {
        for (int i = 0; i < count; i++)
        {
            GameObject resourceObject = GameObject.CreatePrimitive(primitive);
            resourceObject.name = displayName;
            resourceObject.transform.SetParent(root, true);
            resourceObject.transform.position = GetRandomPointInZone(zone);
            resourceObject.transform.localScale = scale * Random.Range(0.85f, 1.55f);
            resourceObject.GetComponent<Renderer>().material.color = color;
            BranchResource resource = resourceObject.AddComponent<BranchResource>();
            resource.Configure(itemId, displayName, duration);
            AddNavIgnore(resourceObject);
        }
    }

    private static void SpawnWaterPatch(DirectorPaintZone zone, Transform root)
    {
        GameObject water = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        water.name = "Director Water Patch";
        water.transform.SetParent(root, true);
        water.transform.position = PlaceOnTerrain(zone.Center) + Vector3.up * 0.03f;
        water.transform.localScale = new Vector3(zone.Radius * 0.18f, 0.035f, zone.Radius * 0.18f);
        water.GetComponent<Renderer>().material.color = new Color(0.10f, 0.34f, 0.76f, 0.82f);
        Object.Destroy(water.GetComponent<Collider>());
    }

    private static Vector3 GetRandomPointInZone(DirectorPaintZone zone)
    {
        Vector2 circle = Random.insideUnitCircle * zone.Radius * 0.78f;
        return PlaceOnTerrain(new Vector3(zone.Center.x + circle.x, 0f, zone.Center.z + circle.y));
    }

    private static void CreatePaintMarker(DirectorZoneType type, Vector3 center, float radius)
    {
        GameObject marker = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        marker.name = "Director Paint " + type;
        marker.transform.SetParent(GetOrCreateMarkerRoot().transform, true);
        marker.transform.position = center + Vector3.up * 0.12f;
        marker.transform.localScale = new Vector3(radius * 0.2f, 0.025f, radius * 0.2f);
        marker.GetComponent<Renderer>().material.color = GetZoneColor(type);
        Object.Destroy(marker.GetComponent<Collider>());
    }

    private static Color GetZoneColor(DirectorZoneType type)
    {
        switch (type)
        {
            case DirectorZoneType.Forest:
                return new Color(0.05f, 0.38f, 0.12f, 0.55f);
            case DirectorZoneType.Mountain:
                return new Color(0.45f, 0.45f, 0.42f, 0.55f);
            case DirectorZoneType.Canyon:
                return new Color(0.55f, 0.24f, 0.12f, 0.55f);
            case DirectorZoneType.River:
                return new Color(0.07f, 0.32f, 0.78f, 0.55f);
            case DirectorZoneType.Wetland:
                return new Color(0.08f, 0.28f, 0.20f, 0.55f);
            default:
                return new Color(0.20f, 0.62f, 0.20f, 0.55f);
        }
    }

    private static GameObject GetOrCreateMarkerRoot()
    {
        GameObject root = GameObject.Find(MarkerRootName);
        return root != null ? root : new GameObject(MarkerRootName);
    }

    private static GameObject GetOrCreateAppliedRoot()
    {
        GameObject root = GameObject.Find(AppliedRootName);
        return root != null ? root : new GameObject(AppliedRootName);
    }

    private static void ClearPaintMarkers()
    {
        GameObject root = GameObject.Find(MarkerRootName);
        if (root != null)
        {
            Object.Destroy(root);
        }
    }

    private static Vector3 PlaceOnTerrain(Vector3 position)
    {
        position.y = EnvironmentRuntimeBootstrap.GetTerrainHeight(position) + 0.2f;
        return position;
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

    private static void RefreshDirectorTerrainWindow()
    {
        if (DirectorTerrainWindow.Instance != null)
        {
            DirectorTerrainWindow.Instance.Refresh();
        }
    }
}
