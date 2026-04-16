using UnityEngine;

public enum WorldBiome
{
    Meadow,
    Wetland,
    RiverBank,
    Hill,
    Mountain,
    Canyon,
    CaveRegion
}

public readonly struct WorldSample
{
    public WorldSample(WorldBiome biome, float elevation01, float steepness, float moisture, bool water, float speedMultiplier, bool walkable)
    {
        Biome = biome;
        Elevation01 = elevation01;
        Steepness = steepness;
        Moisture = moisture;
        Water = water;
        SpeedMultiplier = speedMultiplier;
        Walkable = walkable;
    }

    public WorldBiome Biome { get; }
    public float Elevation01 { get; }
    public float Steepness { get; }
    public float Moisture { get; }
    public bool Water { get; }
    public float SpeedMultiplier { get; }
    public bool Walkable { get; }
}

public static class WorldChunkService
{
    public const float ChunkSize = 125f;
    public const int InitialChunkRadius = 2;

    public static Vector2Int GetChunkCoord(Vector3 worldPosition)
    {
        float halfWorld = EnvironmentRuntimeBootstrap.WorldSize * 0.5f;
        int x = Mathf.FloorToInt((worldPosition.x + halfWorld) / ChunkSize);
        int z = Mathf.FloorToInt((worldPosition.z + halfWorld) / ChunkSize);
        return new Vector2Int(x, z);
    }

    public static Vector3 GetChunkCenter(Vector2Int chunkCoord)
    {
        float halfWorld = EnvironmentRuntimeBootstrap.WorldSize * 0.5f;
        return new Vector3(
            -halfWorld + (chunkCoord.x + 0.5f) * ChunkSize,
            0f,
            -halfWorld + (chunkCoord.y + 0.5f) * ChunkSize);
    }

    public static int GetChunkSeed(Vector2Int chunkCoord)
    {
        unchecked
        {
            int seed = EnvironmentRuntimeBootstrap.GetWorldSeed();
            seed ^= chunkCoord.x * 73856093;
            seed ^= chunkCoord.y * 19349663;
            seed ^= 83492791;
            return seed;
        }
    }

    public static WorldSample Sample(Vector3 worldPosition)
    {
        Vector2 normalized = EnvironmentRuntimeBootstrap.WorldToNormalized(worldPosition);
        Terrain terrain = Terrain.activeTerrain;
        float elevation01 = 0f;
        float steepness = 0f;
        if (terrain != null)
        {
            elevation01 = terrain.terrainData.GetInterpolatedHeight(normalized.x, normalized.y) / EnvironmentRuntimeBootstrap.TerrainHeight;
            steepness = terrain.terrainData.GetSteepness(normalized.x, normalized.y);
        }

        float seedOffset = Mathf.Abs(EnvironmentRuntimeBootstrap.GetWorldSeed() % 100000) * 0.001f;
        float moisture = Mathf.PerlinNoise(normalized.x * 9.4f + 8f + seedOffset, normalized.y * 9.4f + 31f + seedOffset);
        bool water = EnvironmentRuntimeBootstrap.IsWaterAt(worldPosition);
        WorldBiome biome = ClassifyBiome(elevation01, steepness, moisture, water, normalized);
        bool walkable = !water && steepness < 55f;
        float speedMultiplier = CalculateSpeedMultiplier(biome, steepness, water);
        return new WorldSample(biome, elevation01, steepness, moisture, water, speedMultiplier, walkable);
    }

    public static string GetBiomeName(WorldBiome biome)
    {
        switch (biome)
        {
            case WorldBiome.Wetland:
                return "Wetland";
            case WorldBiome.RiverBank:
                return "River Bank";
            case WorldBiome.Hill:
                return "Hill";
            case WorldBiome.Mountain:
                return "Mountain";
            case WorldBiome.Canyon:
                return "Canyon";
            case WorldBiome.CaveRegion:
                return "Cave Region";
            default:
                return "Meadow";
        }
    }

    private static WorldBiome ClassifyBiome(float elevation01, float steepness, float moisture, bool water, Vector2 normalized)
    {
        if (water || moisture > 0.68f && elevation01 < 0.18f)
        {
            return WorldBiome.Wetland;
        }

        if (moisture > 0.52f && elevation01 < 0.20f)
        {
            return WorldBiome.RiverBank;
        }

        if (steepness > 34f || elevation01 > 0.38f)
        {
            return WorldBiome.Mountain;
        }

        float caveNoise = Mathf.PerlinNoise(normalized.x * 13.5f + 47f, normalized.y * 13.5f + 19f);
        if (caveNoise > 0.72f && elevation01 > 0.18f)
        {
            return WorldBiome.CaveRegion;
        }

        if (steepness > 18f || elevation01 > 0.24f)
        {
            return WorldBiome.Hill;
        }

        float canyonNoise = Mathf.PerlinNoise(normalized.x * 7.5f + 101f, normalized.y * 7.5f - 12f);
        if (canyonNoise > 0.76f && elevation01 > 0.12f)
        {
            return WorldBiome.Canyon;
        }

        return WorldBiome.Meadow;
    }

    private static float CalculateSpeedMultiplier(WorldBiome biome, float steepness, bool water)
    {
        if (water)
        {
            return 0f;
        }

        float speed = 1f;
        switch (biome)
        {
            case WorldBiome.Wetland:
                speed = 0.55f;
                break;
            case WorldBiome.RiverBank:
                speed = 0.82f;
                break;
            case WorldBiome.Hill:
                speed = 0.78f;
                break;
            case WorldBiome.Mountain:
                speed = 0.58f;
                break;
            case WorldBiome.Canyon:
            case WorldBiome.CaveRegion:
                speed = 0.68f;
                break;
        }

        if (steepness > 45f)
        {
            speed *= 0.72f;
        }
        else if (steepness > 30f)
        {
            speed *= 0.86f;
        }

        return Mathf.Clamp(speed, 0.25f, 1.15f);
    }
}
