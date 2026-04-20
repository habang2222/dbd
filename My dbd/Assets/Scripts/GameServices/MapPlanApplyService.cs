using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public sealed class MapPlanApplyService : MonoBehaviour
{
    private const string PlanFolderName = "map_plans";
    private const string CurrentPlanFileName = "current_map_plan.json";
    private const string CommandFileName = "map_plan_command.txt";
    private const string StatusFileName = "map_plan_status.json";
    private const string MarkerRootName = "Ops Map Plan Markers";

    private string lastCommandToken;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<MapPlanApplyService>() != null)
        {
            return;
        }

        GameObject serviceObject = new GameObject("Map Plan Apply Service");
        DontDestroyOnLoad(serviceObject);
        serviceObject.AddComponent<MapPlanApplyService>();
    }

    private void Update()
    {
        string commandPath = GetCommandPath();
        if (!File.Exists(commandPath))
        {
            return;
        }

        try
        {
            string token = File.ReadAllText(commandPath).Trim();
            if (string.IsNullOrWhiteSpace(token) || token == lastCommandToken)
            {
                return;
            }

            lastCommandToken = token;
            if (token.StartsWith("clear-world-", StringComparison.Ordinal))
            {
                ClearWorldMapObjects(token);
                return;
            }

            ApplyCurrentPlan(token);
        }
        catch (Exception exception)
        {
            WriteStatus("failed", "map plan command failed: " + exception.Message, 0, 0, 0);
        }
    }

    private static void ApplyCurrentPlan(string token)
    {
        string planPath = GetCurrentPlanPath();
        if (!File.Exists(planPath))
        {
            WriteStatus("failed", "current_map_plan.json missing", 0, 0, 0);
            return;
        }

        ServerBackupService.RequestImmediateBackup("before_map_plan_apply");

        try
        {
            MapPlanDocument plan = JsonUtility.FromJson<MapPlanDocument>(File.ReadAllText(planPath));
            if (plan == null)
            {
                WriteStatus("failed", "map plan parse failed", 0, 0, 0);
                return;
            }

            ClearExistingMarkers();
            GameObject root = new GameObject(MarkerRootName);
            bool hasResourceZones = HasStrokeType(plan.strokes, "resource_zone");
            if (hasResourceZones)
            {
                PlayerPrefs.SetInt(ResourceRuntimeBootstrap.WorldClearedKey, 0);
                PlayerPrefs.Save();
            }

            int resourceCount = ApplyStrokes(plan.strokes, root.transform, "resource_zone");
            int dangerCount = ApplyStrokes(plan.strokes, root.transform, "danger_zone");
            int terrainCount = ApplyHeightStrokes(plan.strokes, root.transform);
            int spawnCount = ApplyMarkers(plan.markers, root.transform);

            WriteStatus("applied", "map plan applied", resourceCount, dangerCount, spawnCount, terrainCount, token);
            ServerBackupService.RequestImmediateBackup("after_map_plan_apply");
        }
        catch (Exception exception)
        {
            WriteStatus("failed", exception.Message, 0, 0, 0, 0, token);
        }
    }

    private static bool HasStrokeType(List<MapPlanStroke> strokes, string type)
    {
        if (strokes == null)
        {
            return false;
        }

        foreach (MapPlanStroke stroke in strokes)
        {
            if (stroke != null && stroke.type == type && stroke.points != null && stroke.points.Count > 0)
            {
                return true;
            }
        }

        return false;
    }

    private static void ClearWorldMapObjects(string token)
    {
        ServerBackupService.RequestImmediateBackup("before_map_clear");
        PlayerPrefs.SetInt(ResourceRuntimeBootstrap.WorldClearedKey, 1);
        PlayerPrefs.Save();
        ResourceRuntimeBootstrap.ClearGeneratedResourceChunks();

        int resources = DestroyAll<BranchResource>();
        int enemies = DestroyAll<EnemyComponent>();
        int planMarkers = DestroyNamedRoot(MarkerRootName);
        int directorMarkers = DestroyNamedRoot("Director Painted Zones");
        int terrainZones = DestroyAll<TerrainZoneInfo>();
        int generatedRoots = DestroyGeneratedWorldRoots();
        int looseResourceObjects = DestroyLooseResourceObjects();

        WriteStatus(
            "cleared",
            "world map objects cleared",
            resources,
            enemies + directorMarkers,
            0,
            terrainZones + planMarkers + generatedRoots + looseResourceObjects,
            token);

        MapPlanApplyService service = FindFirstObjectByType<MapPlanApplyService>();
        if (service != null)
        {
            service.StartCoroutine(BackupAfterClearFrame());
        }
    }

    private static int DestroyGeneratedWorldRoots()
    {
        string[] names =
        {
            "River",
            "Terrain Water",
            "Meadow",
            "Dirt Zone",
            "Sand Zone",
            "Clay Bank",
            "Marsh",
            "Wetland",
            "Pond",
            "Lake",
            "Tree",
            "Waterfall",
            "Canyon",
            "Mountain",
            "High Mountain",
            "Great Mountain",
            "Workbench",
            "Resource Chunk",
            "Starter Wood",
            "Starter Branch",
            "Starter Leaf",
            "Starter Stone",
            "Starter Flint",
            "Starter Dirt",
            "Starter Sand",
            "Ops Planned Resource",
            "Ops Map Plan Markers"
        };

        int count = 0;
        foreach (string name in names)
        {
            foreach (GameObject target in GameObject.FindGameObjectsWithTag("Untagged"))
            {
                if (target != null && target.name.StartsWith(name, StringComparison.Ordinal))
                {
                    Destroy(target);
                    count++;
                }
            }
        }

        return count;
    }

    private static int DestroyLooseResourceObjects()
    {
        string[] names =
        {
            "Leaf",
            "Branch",
            "Wood",
            "Stone",
            "Sand",
            "Dirt",
            "Coal",
            "Copper",
            "Lead",
            "Tin",
            "Iron",
            "Water",
            "Flint"
        };

        int count = 0;
        foreach (GameObject target in GameObject.FindGameObjectsWithTag("Untagged"))
        {
            if (target == null)
            {
                continue;
            }

            for (int i = 0; i < names.Length; i++)
            {
                if (target.name.StartsWith(names[i], StringComparison.Ordinal))
                {
                    Destroy(target);
                    count++;
                    break;
                }
            }
        }

        return count;
    }

    private static IEnumerator BackupAfterClearFrame()
    {
        yield return null;
        yield return null;
        ServerBackupService.RequestImmediateBackup("after_map_clear");
    }

    private static int DestroyAll<T>() where T : Component
    {
        T[] components = FindObjectsByType<T>(FindObjectsSortMode.None);
        int count = 0;
        foreach (T component in components)
        {
            if (component == null)
            {
                continue;
            }

            Destroy(component.gameObject);
            count++;
        }

        return count;
    }

    private static int DestroyNamedRoot(string rootName)
    {
        GameObject root = GameObject.Find(rootName);
        if (root == null)
        {
            return 0;
        }

        Destroy(root);
        return 1;
    }

    private static int ApplyStrokes(List<MapPlanStroke> strokes, Transform root, string type)
    {
        if (strokes == null)
        {
            return 0;
        }

        int count = 0;
        foreach (MapPlanStroke stroke in strokes)
        {
            if (stroke == null || stroke.type != type || stroke.points == null)
            {
                continue;
            }

            foreach (MapPlanPoint point in stroke.points)
            {
                if (point == null)
                {
                    continue;
                }

                Vector3 position = PlaceOnGround(point.x, point.z);
                float radius = Mathf.Clamp(stroke.radius <= 0f ? 24f : stroke.radius, 4f, 120f);
                if (type == "resource_zone")
                {
                    CreateZoneMarker("Resource Zone", position, radius, new Color(0.12f, 0.72f, 0.24f, 0.38f), root);
                    CreateResource(position, count);
                }
                else
                {
                    CreateZoneMarker("Danger Zone", position, radius, new Color(0.90f, 0.12f, 0.12f, 0.42f), root);
                    CreateDangerMarker(position, root);
                }

                count++;
            }
        }

        return count;
    }

    private static int ApplyMarkers(List<MapPlanMarker> markers, Transform root)
    {
        if (markers == null)
        {
            return 0;
        }

        int count = 0;
        foreach (MapPlanMarker marker in markers)
        {
            if (marker == null || marker.type != "spawn_point")
            {
                continue;
            }

            Vector3 position = PlaceOnGround(marker.x, marker.z);
            GameObject spawn = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            spawn.name = string.IsNullOrWhiteSpace(marker.label) ? "Map Plan Spawn" : "Map Plan Spawn " + marker.label;
            spawn.transform.SetParent(root, true);
            spawn.transform.position = position + Vector3.up * 0.2f;
            spawn.transform.localScale = new Vector3(2.8f, 0.25f, 2.8f);
            SetColor(spawn, new Color(0.18f, 0.48f, 1f, 0.85f));
            count++;
        }

        return count;
    }

    private static int ApplyHeightStrokes(List<MapPlanStroke> strokes, Transform root)
    {
        if (strokes == null)
        {
            return 0;
        }

        int count = 0;
        foreach (MapPlanStroke stroke in strokes)
        {
            if (stroke == null || stroke.points == null)
            {
                continue;
            }

            bool supported = stroke.type == "height_raise" || stroke.type == "height_lower";
            if (!supported)
            {
                continue;
            }

            foreach (MapPlanPoint point in stroke.points)
            {
                if (point == null)
                {
                    continue;
                }

                Vector3 position = PlaceOnGround(point.x, point.z);
                float radius = Mathf.Clamp(stroke.radius <= 0f ? 50f : stroke.radius, 8f, 140f);
                float delta = stroke.heightDelta;
                if (Mathf.Approximately(delta, 0f))
                {
                    delta = stroke.type == "height_lower" ? -8f : 8f;
                }

                CreateHeightContour(position, radius, delta, root);

                count++;
            }
        }

        return count;
    }

    private static void CreateZoneMarker(string name, Vector3 position, float radius, Color color, Transform root)
    {
        GameObject marker = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        marker.name = name;
        marker.transform.SetParent(root, true);
        marker.transform.position = position + Vector3.up * 0.08f;
        marker.transform.localScale = new Vector3(radius * 2f, 0.08f, radius * 2f);
        SetColor(marker, color);
    }

    private static void CreateResource(Vector3 position, int index)
    {
        GameObject resourceObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
        resourceObject.name = "Ops Planned Resource";
        resourceObject.transform.position = position + Vector3.up * 0.35f;
        resourceObject.transform.localScale = new Vector3(0.9f, 0.35f, 0.9f);
        SetColor(resourceObject, new Color(0.24f, 0.72f, 0.22f, 1f));
        BranchResource resource = resourceObject.AddComponent<BranchResource>();
        resource.Configure("ops_resource_" + index, "Ops Planned Resource", 4f, 1);
    }

    private static void CreateDangerMarker(Vector3 position, Transform root)
    {
        GameObject danger = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        danger.name = "Ops Danger Marker";
        danger.transform.SetParent(root, true);
        danger.transform.position = position + Vector3.up * 1.1f;
        danger.transform.localScale = Vector3.one * 2.2f;
        SetColor(danger, new Color(0.9f, 0.1f, 0.1f, 1f));
    }

    private static void CreateHeightContour(Vector3 position, float radius, float heightDelta, Transform root)
    {
        float height = Mathf.Abs(heightDelta);
        GameObject marker = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        marker.name = heightDelta >= 0f ? "Ops Height Raise" : "Ops Height Lower";
        marker.transform.SetParent(root, true);
        marker.transform.position = position + Vector3.up * (heightDelta >= 0f ? height * 0.18f : 0.04f);
        marker.transform.localScale = new Vector3(radius * 2f, Mathf.Max(0.06f, height * 0.18f), radius * 2f);
        SetColor(marker, HeightColor(heightDelta));

        for (int i = 1; i <= 3; i++)
        {
            GameObject ring = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            ring.name = "Ops Contour Ring";
            ring.transform.SetParent(root, true);
            ring.transform.position = position + Vector3.up * (0.12f + i * 0.03f);
            float ringRadius = radius * (i / 3f);
            ring.transform.localScale = new Vector3(ringRadius * 2f, 0.025f, ringRadius * 2f);
            SetColor(ring, HeightColor(heightDelta * (i / 3f)));
        }
    }

    private static Color HeightColor(float heightDelta)
    {
        if (heightDelta < 0f)
        {
            float t = Mathf.Clamp01(Mathf.Abs(heightDelta) / 18f);
            return Color.Lerp(new Color(0.02f, 0.02f, 0.025f, 0.78f), new Color(0f, 0f, 0f, 0.9f), t);
        }

        if (heightDelta < 6f)
        {
            return new Color(0.05f, 0.28f, 0.85f, 0.72f);
        }

        float high = Mathf.Clamp01((heightDelta - 6f) / 18f);
        return Color.Lerp(new Color(0.05f, 0.28f, 0.85f, 0.72f), new Color(0.92f, 0.06f, 0.04f, 0.82f), high);
    }

    private static Vector3 PlaceOnGround(float x, float z)
    {
        Vector3 origin = new Vector3(x, 2000f, z);
        if (Physics.Raycast(origin, Vector3.down, out RaycastHit hit, 4000f))
        {
            return hit.point;
        }

        return new Vector3(x, 0f, z);
    }

    private static void SetColor(GameObject target, Color color)
    {
        Renderer renderer = target.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.material.color = color;
        }
    }

    private static void ClearExistingMarkers()
    {
        GameObject existing = GameObject.Find(MarkerRootName);
        if (existing != null)
        {
            Destroy(existing);
        }
    }

    private static void WriteStatus(string status, string message, int resourceZones, int dangerZones, int spawnPoints, int terrainFeatures = 0, string token = "")
    {
        MapPlanStatus result = new MapPlanStatus
        {
            status = status,
            message = message,
            utcTime = DateTime.UtcNow.ToString("O"),
            token = token,
            resourceZones = resourceZones,
            dangerZones = dangerZones,
            spawnPoints = spawnPoints,
            terrainFeatures = terrainFeatures
        };

        File.WriteAllText(GetStatusPath(), JsonUtility.ToJson(result, true));
    }

    private static string GetCurrentPlanPath()
    {
        return Path.Combine(Application.persistentDataPath, PlanFolderName, CurrentPlanFileName);
    }

    private static string GetCommandPath()
    {
        return Path.Combine(Application.persistentDataPath, CommandFileName);
    }

    private static string GetStatusPath()
    {
        return Path.Combine(Application.persistentDataPath, StatusFileName);
    }
}

[Serializable]
public sealed class MapPlanDocument
{
    public string version = "1";
    public string createdAt;
    public List<MapPlanStroke> strokes = new();
    public List<MapPlanMarker> markers = new();
}

[Serializable]
public sealed class MapPlanStroke
{
    public string type;
    public string brush;
    public float radius;
    public float heightDelta;
    public List<MapPlanPoint> points = new();
}

[Serializable]
public sealed class MapPlanMarker
{
    public string type;
    public string label;
    public float x;
    public float z;
}

[Serializable]
public sealed class MapPlanPoint
{
    public float x;
    public float z;
}

[Serializable]
public sealed class MapPlanStatus
{
    public string status;
    public string message;
    public string utcTime;
    public string token;
    public int resourceZones;
    public int dangerZones;
    public int spawnPoints;
    public int terrainFeatures;
}
