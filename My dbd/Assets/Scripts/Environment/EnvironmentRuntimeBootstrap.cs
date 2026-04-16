using Unity.AI.Navigation;
using UnityEngine;
using UnityEngine.AI;

public static class EnvironmentRuntimeBootstrap
{
    public const float WorldSize = 2000f;
    public const float TerrainHeight = 170f;

    private const int HeightmapResolution = 513;
    private const int AlphamapResolution = 256;
    private const string WorldGenerationSeedKey = "DBD.WorldGenerationSeed";
    private static readonly Vector3 TerrainOrigin = new(-WorldSize * 0.5f, 0f, -WorldSize * 0.5f);
    private static bool terrainBuilt;
    private static bool navMeshBuilt;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateEnvironmentIfMissing()
    {
        EnsureEnvironment();
    }

    public static void EnsureEnvironment()
    {
        CreateTerrainGround();
        CreateObstacleIfMissing();
        RebuildNavMesh();
    }

    public static int GetWorldSeed()
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

    public static Vector2 WorldToNormalized(Vector3 worldPosition)
    {
        return new Vector2(
            Mathf.InverseLerp(TerrainOrigin.x, TerrainOrigin.x + WorldSize, worldPosition.x),
            Mathf.InverseLerp(TerrainOrigin.z, TerrainOrigin.z + WorldSize, worldPosition.z));
    }

    public static float GetTerrainHeight(Vector3 worldPosition)
    {
        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return 0f;
        }

        return terrain.SampleHeight(worldPosition) + terrain.transform.position.y;
    }

    public static bool IsWaterAt(Vector3 worldPosition)
    {
        Vector2 normalized = WorldToNormalized(worldPosition);
        return GetRiverDistance01(normalized) < 0.010f
            || Distance01(normalized, new Vector2(0.28f, 0.72f), new Vector2(0.045f, 0.030f)) < 1f
            || Distance01(normalized, new Vector2(0.62f, 0.66f), new Vector2(0.065f, 0.040f)) < 1f;
    }

    public static float GetTerrainSpeedMultiplier(Vector3 worldPosition)
    {
        return WorldChunkService.Sample(worldPosition).SpeedMultiplier;
    }

    public static bool CanStandAt(Vector3 worldPosition)
    {
        return WorldChunkService.Sample(worldPosition).Walkable;
    }

    private static void CreateTerrainGround()
    {
        GameObject oldGround = GameObject.Find("Ground");
        Terrain terrain = oldGround != null ? oldGround.GetComponent<Terrain>() : null;
        if (terrainBuilt && terrain != null)
        {
            ConfigureSurface(terrain.gameObject);
            return;
        }

        if (terrain == null)
        {
            if (oldGround != null)
            {
                Object.Destroy(oldGround);
            }

            TerrainData terrainData = new TerrainData
            {
                heightmapResolution = HeightmapResolution,
                alphamapResolution = AlphamapResolution,
                size = new Vector3(WorldSize, TerrainHeight, WorldSize)
            };

            GameObject terrainObject = Terrain.CreateTerrainGameObject(terrainData);
            terrainObject.name = "Ground";
            terrainObject.transform.position = TerrainOrigin;
            terrain = terrainObject.GetComponent<Terrain>();
        }

        terrain.transform.position = TerrainOrigin;
        terrain.terrainData.heightmapResolution = HeightmapResolution;
        terrain.terrainData.alphamapResolution = AlphamapResolution;
        terrain.terrainData.size = new Vector3(WorldSize, TerrainHeight, WorldSize);
        terrain.terrainData.terrainLayers = CreateTerrainLayers();
        terrain.terrainData.SetHeights(0, 0, GenerateHeights(GetWorldSeed()));
        terrain.terrainData.SetAlphamaps(0, 0, GenerateAlphamaps(terrain.terrainData));

        TerrainCollider terrainCollider = terrain.GetComponent<TerrainCollider>();
        if (terrainCollider != null)
        {
            terrainCollider.terrainData = terrain.terrainData;
        }

        ConfigureSurface(terrain.gameObject);
        CreateWaterSurfaces();
        terrainBuilt = true;
    }

    private static float[,] GenerateHeights(int seed)
    {
        float[,] heights = new float[HeightmapResolution, HeightmapResolution];
        float seedOffset = Mathf.Abs(seed % 100000) * 0.001f;

        for (int z = 0; z < HeightmapResolution; z++)
        {
            float v = z / (float)(HeightmapResolution - 1);
            for (int x = 0; x < HeightmapResolution; x++)
            {
                float u = x / (float)(HeightmapResolution - 1);
                float plains = Mathf.PerlinNoise(u * 5.5f + seedOffset, v * 5.5f + seedOffset);
                float hills = Mathf.PerlinNoise(u * 8f + seedOffset * 0.7f, v * 8f + seedOffset * 0.7f);
                float mountains = Mathf.PerlinNoise(u * 2.2f + 90f + seedOffset, v * 2.2f + 14f + seedOffset);
                float ridges = 1f - Mathf.Abs(Mathf.PerlinNoise(u * 4.5f + seedOffset, v * 4.5f - seedOffset) * 2f - 1f);

                float height = 0.030f + plains * 0.035f + hills * 0.026f;
                if (mountains > 0.58f)
                {
                    height += Mathf.Pow((mountains - 0.58f) / 0.42f, 1.9f) * 0.34f;
                    height += ridges * 0.055f;
                }

                float riverDistance = GetRiverDistance01(new Vector2(u, v));
                if (riverDistance < 0.020f)
                {
                    height = Mathf.Min(height, Mathf.Lerp(0.020f, 0.042f, riverDistance / 0.020f));
                }
                else if (riverDistance < 0.050f)
                {
                    height *= Mathf.Lerp(0.70f, 1f, (riverDistance - 0.020f) / 0.030f);
                }

                height = CarveLake(height, new Vector2(u, v), new Vector2(0.28f, 0.72f), new Vector2(0.045f, 0.030f), 0.026f);
                height = CarveLake(height, new Vector2(u, v), new Vector2(0.62f, 0.66f), new Vector2(0.065f, 0.040f), 0.023f);
                heights[z, x] = Mathf.Clamp01(height);
            }
        }

        return SmoothHeights(heights, 3);
    }

    private static TerrainLayer[] CreateTerrainLayers()
    {
        return new[]
        {
            CreateLayer(new Color(0.24f, 0.48f, 0.20f, 1f)),
            CreateLayer(new Color(0.78f, 0.58f, 0.25f, 1f)),
            CreateLayer(new Color(0.34f, 0.35f, 0.34f, 1f)),
            CreateLayer(new Color(0.12f, 0.35f, 0.90f, 1f)),
            CreateLayer(new Color(0.18f, 0.30f, 0.16f, 1f))
        };
    }

    private static TerrainLayer CreateLayer(Color color)
    {
        Texture2D texture = new Texture2D(1, 1);
        texture.SetPixel(0, 0, color);
        texture.Apply();
        TerrainLayer layer = new TerrainLayer
        {
            diffuseTexture = texture,
            tileSize = new Vector2(20f, 20f)
        };
        return layer;
    }

    private static float[,] SmoothHeights(float[,] source, int passes)
    {
        int height = source.GetLength(0);
        int width = source.GetLength(1);
        float[,] current = source;

        for (int pass = 0; pass < passes; pass++)
        {
            float[,] next = new float[height, width];
            for (int z = 0; z < height; z++)
            {
                for (int x = 0; x < width; x++)
                {
                    float total = 0f;
                    int count = 0;
                    for (int dz = -1; dz <= 1; dz++)
                    {
                        int sampleZ = Mathf.Clamp(z + dz, 0, height - 1);
                        for (int dx = -1; dx <= 1; dx++)
                        {
                            int sampleX = Mathf.Clamp(x + dx, 0, width - 1);
                            total += current[sampleZ, sampleX];
                            count++;
                        }
                    }

                    next[z, x] = total / count;
                }
            }

            current = next;
        }

        return current;
    }

    private static float[,,] GenerateAlphamaps(TerrainData terrainData)
    {
        int width = terrainData.alphamapWidth;
        int height = terrainData.alphamapHeight;
        float[,,] maps = new float[height, width, 5];

        for (int y = 0; y < height; y++)
        {
            float v = y / (float)(height - 1);
            for (int x = 0; x < width; x++)
            {
                float u = x / (float)(width - 1);
                float elevation = terrainData.GetInterpolatedHeight(u, v) / TerrainHeight;
                float steepness = terrainData.GetSteepness(u, v);
                float riverDistance = GetRiverDistance01(new Vector2(u, v));
                float moisture = Mathf.PerlinNoise(u * 9.4f + 8f, v * 9.4f + 31f);

                float water = Mathf.Clamp01(1f - riverDistance / 0.024f);
                water = Mathf.Max(water, 1f - Distance01(new Vector2(u, v), new Vector2(0.28f, 0.72f), new Vector2(0.045f, 0.030f)));
                water = Mathf.Max(water, 1f - Distance01(new Vector2(u, v), new Vector2(0.62f, 0.66f), new Vector2(0.065f, 0.040f)));
                float sand = Mathf.Clamp01(1f - Mathf.Abs(riverDistance - 0.040f) / 0.035f) * (1f - water);
                float rock = Mathf.Clamp01((steepness - 20f) / 30f) + Mathf.Clamp01((elevation - 0.28f) / 0.18f);
                float marsh = Mathf.Clamp01((moisture - 0.66f) / 0.22f) * Mathf.Clamp01((0.18f - elevation) / 0.12f) * (1f - water);
                float grass = Mathf.Max(0.1f, 1f - water - sand - rock * 0.6f - marsh * 0.7f);

                maps[y, x, 0] = grass;
                maps[y, x, 1] = sand;
                maps[y, x, 2] = rock;
                maps[y, x, 3] = water;
                maps[y, x, 4] = marsh;

                float total = maps[y, x, 0] + maps[y, x, 1] + maps[y, x, 2] + maps[y, x, 3] + maps[y, x, 4];
                for (int layer = 0; layer < 5; layer++)
                {
                    maps[y, x, layer] /= total;
                }
            }
        }

        return maps;
    }

    private static void CreateWaterSurfaces()
    {
        foreach (GameObject water in GameObject.FindGameObjectsWithTag("Untagged"))
        {
            if (water.name == "Terrain Water")
            {
                Object.Destroy(water);
            }
        }

        CreateWaterPlane("Terrain Water", new Vector3(0f, 6.2f, 0f), new Vector3(WorldSize * 0.92f, 1f, 34f), 0f);
        CreateWaterPlane("Terrain Water", new Vector3(-440f, 5.6f, 440f), new Vector3(100f, 1f, 62f), -16f);
        CreateWaterPlane("Terrain Water", new Vector3(240f, 5.2f, 330f), new Vector3(145f, 1f, 82f), 12f);
    }

    private static void CreateWaterPlane(string name, Vector3 position, Vector3 scale, float rotationY)
    {
        GameObject water = GameObject.CreatePrimitive(PrimitiveType.Cube);
        water.name = name;
        water.transform.position = position;
        water.transform.localScale = new Vector3(scale.x, 0.08f, scale.z);
        water.transform.rotation = Quaternion.Euler(0f, rotationY, 0f);
        water.GetComponent<Renderer>().material.color = new Color(0.10f, 0.35f, 0.85f, 0.82f);
        Object.Destroy(water.GetComponent<Collider>());
    }

    private static void CreateObstacleIfMissing()
    {
        GameObject obstacle = GameObject.Find("Obstacle_1");
        if (obstacle != null)
        {
            EnsureObstacleComponents(obstacle);
            return;
        }

        obstacle = GameObject.CreatePrimitive(PrimitiveType.Cube);
        obstacle.name = "Obstacle_1";
        obstacle.transform.position = new Vector3(0f, GetTerrainHeight(Vector3.zero) + 0.75f, 4f);
        obstacle.transform.localScale = new Vector3(1.5f, 1.5f, 1.5f);
        obstacle.GetComponent<Renderer>().material.color = new Color(0.45f, 0.42f, 0.38f);
        EnsureObstacleComponents(obstacle);
    }

    private static void EnsureObstacleComponents(GameObject obstacle)
    {
        if (obstacle.GetComponent<ObstacleMarker>() == null)
        {
            obstacle.AddComponent<ObstacleMarker>();
        }

        NavMeshObstacle navObstacle = obstacle.GetComponent<NavMeshObstacle>();
        if (navObstacle == null)
        {
            navObstacle = obstacle.AddComponent<NavMeshObstacle>();
        }

        navObstacle.carving = true;
        navObstacle.carveOnlyStationary = false;
        navObstacle.shape = NavMeshObstacleShape.Box;
        navObstacle.center = Vector3.zero;
        navObstacle.size = obstacle.transform.localScale + new Vector3(0.4f, 0f, 0.4f);

        NavMeshModifier modifier = obstacle.GetComponent<NavMeshModifier>();
        if (modifier == null)
        {
            modifier = obstacle.AddComponent<NavMeshModifier>();
        }

        modifier.overrideArea = true;
        modifier.area = 1;
    }

    private static void RebuildNavMesh()
    {
        if (navMeshBuilt)
        {
            return;
        }

        NavMeshSurface surface = Object.FindFirstObjectByType<NavMeshSurface>();
        if (surface != null)
        {
            surface.BuildNavMesh();
            navMeshBuilt = true;
        }
    }

    private static void ConfigureSurface(GameObject ground)
    {
        NavMeshSurface surface = ground.GetComponent<NavMeshSurface>();
        if (surface == null)
        {
            surface = ground.AddComponent<NavMeshSurface>();
        }

        surface.collectObjects = CollectObjects.Volume;
        surface.center = new Vector3(WorldSize * 0.5f, TerrainHeight * 0.5f, WorldSize * 0.5f);
        surface.size = new Vector3(WorldSize, TerrainHeight + 40f, WorldSize);
    }

    private static float GetRiverDistance01(Vector2 point)
    {
        float riverCenter = 0.50f + Mathf.Sin(point.x * Mathf.PI * 3.2f) * 0.085f + Mathf.Cos(point.x * Mathf.PI * 1.4f) * 0.035f;
        return Mathf.Abs(point.y - riverCenter);
    }

    private static float CarveLake(float currentHeight, Vector2 point, Vector2 center, Vector2 radius, float lakeHeight)
    {
        float distance = Distance01(point, center, radius);
        if (distance > 1f)
        {
            return currentHeight;
        }

        return Mathf.Min(currentHeight, Mathf.Lerp(lakeHeight, currentHeight, distance));
    }

    private static float Distance01(Vector2 point, Vector2 center, Vector2 radius)
    {
        float dx = (point.x - center.x) / Mathf.Max(0.001f, radius.x);
        float dy = (point.y - center.y) / Mathf.Max(0.001f, radius.y);
        return Mathf.Sqrt(dx * dx + dy * dy);
    }
}
