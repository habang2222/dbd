using System.Collections.Generic;
using Unity.AI.Navigation;
using UnityEngine;
using UnityEngine.EventSystems;

public class DirectorIntentBrushSystem : MonoBehaviour
{
    public static DirectorIntentBrushSystem Instance { get; private set; }

    private const float MinBrushRadius = 18f;
    private const float MaxBrushRadius = 220f;
    private const float DetailDensityScale = 0.00055f;

    private readonly List<DirectorIntentStroke> pendingStrokes = new();
    private readonly List<GameObject> previewObjects = new();

    [SerializeField] private DirectorIntentType currentIntent = DirectorIntentType.Meadow;
    [SerializeField] private bool paintEnabled;
    [SerializeField] private float brushRadius = 70f;
    [SerializeField] private float brushStrength = 0.75f;
    [SerializeField] private float rayDistance = 20000f;

    public DirectorIntentType CurrentIntent => currentIntent;
    public bool PaintEnabled => paintEnabled;
    public float BrushRadius => brushRadius;
    public float BrushStrength => brushStrength;
    public int PendingStrokeCount => pendingStrokes.Count;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<DirectorIntentBrushSystem>() != null)
        {
            return;
        }

        GameObject brushObject = new GameObject("Director Intent Brush System");
        brushObject.AddComponent<DirectorIntentBrushSystem>();
    }

    private void Awake()
    {
        Instance = this;
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    private void Update()
    {
        if (!SessionRoleService.IsDirector || !paintEnabled || IsPointerOverUi())
        {
            return;
        }

        if (Input.GetMouseButton(0) && TryGetWorldPoint(out Vector3 point))
        {
            Paint(point);
        }
    }

    public void SelectBrush(DirectorIntentType intent)
    {
        if (intent != DirectorIntentType.Meadow && intent != DirectorIntentType.Forest)
        {
            paintEnabled = false;
            currentIntent = DirectorIntentType.Meadow;
            RefreshTerrainWindow();
            return;
        }

        currentIntent = intent;
        paintEnabled = true;
        if (DirectorWorldTool.Instance != null)
        {
            DirectorWorldTool.Instance.SetMode(DirectorToolMode.None);
        }

        RefreshTerrainWindow();
    }

    public void SetBrushRadius(float radius)
    {
        brushRadius = Mathf.Clamp(radius, MinBrushRadius, MaxBrushRadius);
        RefreshTerrainWindow();
    }

    public void AddBrushRadius(float amount)
    {
        SetBrushRadius(brushRadius + amount);
    }

    public void SetBrushStrength(float strength)
    {
        brushStrength = Mathf.Clamp01(strength);
        RefreshTerrainWindow();
    }

    public void Push()
    {
        if (pendingStrokes.Count == 0)
        {
            return;
        }

        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return;
        }

        GameObject detailRoot = GetOrCreateDetailRoot();
        for (int i = 0; i < pendingStrokes.Count; i++)
        {
            ApplyStrokeToTerrain(terrain, pendingStrokes[i]);
            PaintStrokeTexture(terrain, pendingStrokes[i]);
            SpawnPostProcessDetails(pendingStrokes[i], detailRoot.transform);
        }

        ClearPendingPreviews();
        RebuildDirectorNavMesh();
        RefreshTerrainWindow();
    }

    public void Finish()
    {
        Push();
        paintEnabled = false;
        ServerWorldStateService.OpenToPlayers();
        if (DirectorWorldTool.Instance != null)
        {
            DirectorWorldTool.Instance.SetMode(DirectorToolMode.None);
        }

        RefreshTerrainWindow();
    }

    public void ClearPending()
    {
        ClearPendingPreviews();
        RefreshTerrainWindow();
    }

    public void Cancel()
    {
        paintEnabled = false;
        ClearPendingPreviews();
        RefreshTerrainWindow();
    }

    private void Paint(Vector3 center)
    {
        if (pendingStrokes.Count > 0)
        {
            DirectorIntentStroke last = pendingStrokes[pendingStrokes.Count - 1];
            float minSpacing = Mathf.Max(6f, brushRadius * 0.35f);
            if (Vector2.Distance(new Vector2(last.Center.x, last.Center.z), new Vector2(center.x, center.z)) < minSpacing)
            {
                return;
            }
        }

        DirectorIntentStroke stroke = new DirectorIntentStroke(center, brushRadius, brushStrength, currentIntent);
        pendingStrokes.Add(stroke);
        previewObjects.Add(CreatePreview(stroke));
        RefreshTerrainWindow();
    }

    private static void ApplyStrokeToTerrain(Terrain terrain, DirectorIntentStroke stroke)
    {
        TerrainData data = terrain.terrainData;
        if (!TryGetHeightRegion(terrain, stroke.Center, stroke.Radius, out int startX, out int startZ, out int width, out int height, out int centerX, out int centerZ, out int radiusSamples))
        {
            return;
        }

        float[,] heights = data.GetHeights(startX, startZ, width, height);
        float centerHeight = data.GetHeight(centerX, centerZ) / data.size.y;
        float strength = Mathf.Lerp(0.25f, 1f, stroke.Strength);

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
                heights[z, x] = GetIntentHeight(stroke.Intent, heights[z, x], centerHeight, falloff, noise, strength);
            }
        }

        data.SetHeights(startX, startZ, heights);
    }

    private static float GetIntentHeight(DirectorIntentType intent, float current, float centerHeight, float falloff, float noise, float strength)
    {
        switch (intent)
        {
            case DirectorIntentType.Meadow:
                return Mathf.Clamp01(Mathf.Lerp(current, Mathf.Min(centerHeight, 0.09f) + noise * 0.006f, falloff * 0.55f * strength));
            case DirectorIntentType.Forest:
                return Mathf.Clamp01(Mathf.Lerp(current, centerHeight + (noise - 0.5f) * 0.018f, falloff * 0.35f * strength));
            case DirectorIntentType.Mountain:
                return Mathf.Clamp01(current + Mathf.Pow(falloff, 1.7f) * (0.20f + noise * 0.07f) * strength);
            case DirectorIntentType.Canyon:
            {
                float rim = Mathf.Clamp01(1f - Mathf.Abs(falloff - 0.28f) / 0.10f) * 0.045f;
                return Mathf.Clamp01(current - Mathf.Pow(falloff, 0.75f) * 0.18f * strength + rim * strength);
            }
            case DirectorIntentType.River:
                return Mathf.Clamp01(Mathf.Lerp(current, 0.028f + noise * 0.004f, falloff * 0.82f * strength));
            case DirectorIntentType.Wetland:
                return Mathf.Clamp01(Mathf.Lerp(current, 0.045f + noise * 0.010f, falloff * 0.65f * strength));
            default:
                return current;
        }
    }

    private static void PaintStrokeTexture(Terrain terrain, DirectorIntentStroke stroke)
    {
        TerrainData data = terrain.terrainData;
        if (data.alphamapLayers < 5)
        {
            return;
        }

        Vector3 local = stroke.Center - terrain.transform.position;
        int centerX = Mathf.RoundToInt(local.x / data.size.x * (data.alphamapWidth - 1));
        int centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.alphamapHeight - 1));
        int radiusSamples = Mathf.Max(1, Mathf.RoundToInt(stroke.Radius / data.size.x * data.alphamapWidth));
        int startX = Mathf.Clamp(centerX - radiusSamples, 0, data.alphamapWidth - 1);
        int startZ = Mathf.Clamp(centerZ - radiusSamples, 0, data.alphamapHeight - 1);
        int width = Mathf.Clamp(centerX + radiusSamples, 0, data.alphamapWidth - 1) - startX + 1;
        int height = Mathf.Clamp(centerZ + radiusSamples, 0, data.alphamapHeight - 1) - startZ + 1;
        float[,,] maps = data.GetAlphamaps(startX, startZ, width, height);
        int targetLayer = GetTextureLayer(stroke.Intent);

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

                float falloff = Mathf.SmoothStep(1f, 0f, distance) * Mathf.Lerp(0.35f, 0.85f, stroke.Strength);
                for (int layer = 0; layer < data.alphamapLayers; layer++)
                {
                    maps[z, x, layer] = Mathf.Lerp(maps[z, x, layer], layer == targetLayer ? 1f : 0f, falloff);
                }
            }
        }

        data.SetAlphamaps(startX, startZ, maps);
    }

    private static int GetTextureLayer(DirectorIntentType intent)
    {
        switch (intent)
        {
            case DirectorIntentType.Mountain:
            case DirectorIntentType.Canyon:
                return 2;
            case DirectorIntentType.River:
                return 3;
            case DirectorIntentType.Wetland:
                return 4;
            default:
                return 0;
        }
    }

    private static void SpawnPostProcessDetails(DirectorIntentStroke stroke, Transform parent)
    {
        if (ResourceRuntimeBootstrap.IsWorldCleared)
        {
            return;
        }

        Random.State previousState = Random.state;
        Random.InitState(GetStrokeSeed(stroke));

        int count = Mathf.Clamp(Mathf.RoundToInt(stroke.Radius * stroke.Radius * DetailDensityScale * Mathf.Lerp(0.35f, 1.2f, stroke.Strength)), 2, 36);
        for (int i = 0; i < count; i++)
        {
            Vector2 offset = Random.insideUnitCircle * stroke.Radius * 0.88f;
            Vector3 position = new Vector3(stroke.Center.x + offset.x, 0f, stroke.Center.z + offset.y);
            position.y = EnvironmentRuntimeBootstrap.GetTerrainHeight(position) + 0.2f;
            SpawnDetailForIntent(stroke.Intent, position, parent, i);
        }

        if (stroke.Intent == DirectorIntentType.River)
        {
            CreateWaterPatch(stroke, parent);
        }

        Random.state = previousState;
    }

    private static void SpawnDetailForIntent(DirectorIntentType intent, Vector3 position, Transform parent, int index)
    {
        switch (intent)
        {
            case DirectorIntentType.Forest:
                if (Random.value < 0.72f)
                {
                    CreateTree(position, parent);
                }
                else
                {
                    CreateGatherable(position, parent, "branch_1", "Forest Branch", 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.85f, 0.14f, 0.22f));
                }
                break;
            case DirectorIntentType.Mountain:
                CreateGatherable(position, parent, Random.value < 0.18f ? "iron_1" : "stone_1", Random.value < 0.18f ? "Mountain Iron" : "Mountain Stone", 24f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(0.9f, 0.55f, 0.8f));
                break;
            case DirectorIntentType.Canyon:
                CreateGatherable(position, parent, Random.value < 0.45f ? "coal_1" : "flint_1", Random.value < 0.45f ? "Canyon Coal" : "Canyon Flint", 18f, new Color(0.12f, 0.11f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.65f, 0.42f, 0.55f));
                break;
            case DirectorIntentType.River:
                CreateGatherable(position, parent, Random.value < 0.55f ? "sand_1" : "water_1", Random.value < 0.55f ? "River Sand" : "River Water", 1.2f, new Color(0.20f, 0.45f, 0.85f, 1f), PrimitiveType.Sphere, new Vector3(0.65f, 0.16f, 0.65f));
                break;
            case DirectorIntentType.Wetland:
                CreateGatherable(position, parent, Random.value < 0.65f ? "leaf_1" : "water_1", Random.value < 0.65f ? "Wetland Leaf" : "Wetland Water", 0.6f, new Color(0.16f, 0.45f, 0.18f, 1f), PrimitiveType.Sphere, new Vector3(0.58f, 0.10f, 0.46f));
                break;
            default:
                if (index % 3 == 0)
                {
                    CreateGatherable(position, parent, "branch_1", "Meadow Branch", 0.1f, new Color(0.45f, 0.25f, 0.10f, 1f), PrimitiveType.Cube, new Vector3(0.75f, 0.12f, 0.18f));
                }
                break;
        }
    }

    private static void CreateTree(Vector3 position, Transform parent)
    {
        GameObject treeRoot = new GameObject("Intent Tree");
        treeRoot.transform.SetParent(parent, true);
        treeRoot.transform.position = position;
        float size = Random.Range(0.8f, 1.55f);
        treeRoot.transform.localScale = Vector3.one * size;

        GameObject trunk = GameObject.CreatePrimitive(PrimitiveType.Cube);
        trunk.name = "Intent Tree Trunk";
        trunk.transform.SetParent(treeRoot.transform, false);
        trunk.transform.localPosition = new Vector3(0f, 1.2f, 0f);
        trunk.transform.localScale = new Vector3(0.55f, 2.4f, 0.55f);
        trunk.GetComponent<Renderer>().material.color = new Color(0.36f, 0.20f, 0.09f, 1f);

        GameObject leaves = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        leaves.name = "Intent Tree Crown";
        leaves.transform.SetParent(treeRoot.transform, false);
        leaves.transform.localPosition = new Vector3(0f, 2.8f, 0f);
        leaves.transform.localScale = new Vector3(2.2f, 1.3f, 2.2f);
        leaves.GetComponent<Renderer>().material.color = new Color(0.12f, 0.46f, 0.13f, 1f);
        Destroy(leaves.GetComponent<Collider>());

        BoxCollider collider = treeRoot.AddComponent<BoxCollider>();
        collider.center = new Vector3(0f, 1.4f, 0f);
        collider.size = new Vector3(2.2f, 3f, 2.2f);
        BranchResource resource = treeRoot.AddComponent<BranchResource>();
        resource.ConfigureYields("Intent Tree", 9f * size, new[]
        {
            new PersonInventoryItem("wood_1", 2),
            new PersonInventoryItem("branch_1", 3),
            new PersonInventoryItem("leaf_1", 4)
        });
        AddNavIgnore(treeRoot);
    }

    private static void CreateGatherable(Vector3 position, Transform parent, string itemId, string displayName, float duration, Color color, PrimitiveType primitive, Vector3 scale)
    {
        GameObject detail = GameObject.CreatePrimitive(primitive);
        detail.name = displayName;
        detail.transform.SetParent(parent, true);
        detail.transform.position = position;
        detail.transform.localScale = scale * Random.Range(0.8f, 1.6f);
        detail.GetComponent<Renderer>().material.color = color;
        BranchResource resource = detail.AddComponent<BranchResource>();
        resource.Configure(itemId, displayName, duration);
        AddNavIgnore(detail);
    }

    private static void CreateWaterPatch(DirectorIntentStroke stroke, Transform parent)
    {
        GameObject water = GameObject.CreatePrimitive(PrimitiveType.Cube);
        water.name = "Intent Water";
        water.transform.SetParent(parent, true);
        water.transform.position = new Vector3(stroke.Center.x, EnvironmentRuntimeBootstrap.GetTerrainHeight(stroke.Center) + 0.08f, stroke.Center.z);
        water.transform.localScale = new Vector3(stroke.Radius * 1.55f, 0.05f, stroke.Radius * 1.05f);
        water.transform.rotation = Quaternion.Euler(0f, Random.Range(-18f, 18f), 0f);
        water.GetComponent<Renderer>().material.color = new Color(0.10f, 0.36f, 0.86f, 0.78f);
        Destroy(water.GetComponent<Collider>());
    }

    private GameObject CreatePreview(DirectorIntentStroke stroke)
    {
        GameObject preview = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        preview.name = "Intent Brush Preview";
        preview.transform.position = new Vector3(stroke.Center.x, EnvironmentRuntimeBootstrap.GetTerrainHeight(stroke.Center) + 0.12f, stroke.Center.z);
        preview.transform.localScale = new Vector3(stroke.Radius * 2f, 0.025f, stroke.Radius * 2f);
        preview.GetComponent<Renderer>().material.color = GetPreviewColor(stroke.Intent);
        Destroy(preview.GetComponent<Collider>());
        return preview;
    }

    private static Color GetPreviewColor(DirectorIntentType intent)
    {
        switch (intent)
        {
            case DirectorIntentType.Forest:
                return new Color(0.05f, 0.42f, 0.12f, 0.38f);
            case DirectorIntentType.Mountain:
                return new Color(0.46f, 0.46f, 0.44f, 0.42f);
            case DirectorIntentType.Canyon:
                return new Color(0.55f, 0.20f, 0.12f, 0.42f);
            case DirectorIntentType.River:
                return new Color(0.08f, 0.32f, 0.90f, 0.42f);
            case DirectorIntentType.Wetland:
                return new Color(0.12f, 0.28f, 0.16f, 0.42f);
            default:
                return new Color(0.24f, 0.62f, 0.18f, 0.35f);
        }
    }

    private void ClearPendingPreviews()
    {
        pendingStrokes.Clear();
        for (int i = 0; i < previewObjects.Count; i++)
        {
            if (previewObjects[i] != null)
            {
                Destroy(previewObjects[i]);
            }
        }

        previewObjects.Clear();
    }

    private static bool TryGetHeightRegion(Terrain terrain, Vector3 worldCenter, float radius, out int startX, out int startZ, out int width, out int height, out int centerX, out int centerZ, out int radiusSamples)
    {
        TerrainData data = terrain.terrainData;
        Vector3 local = worldCenter - terrain.transform.position;
        centerX = Mathf.RoundToInt(local.x / data.size.x * (data.heightmapResolution - 1));
        centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.heightmapResolution - 1));
        radiusSamples = Mathf.Max(1, Mathf.RoundToInt(radius / data.size.x * data.heightmapResolution));
        startX = Mathf.Clamp(centerX - radiusSamples, 0, data.heightmapResolution - 1);
        startZ = Mathf.Clamp(centerZ - radiusSamples, 0, data.heightmapResolution - 1);
        width = Mathf.Clamp(centerX + radiusSamples, 0, data.heightmapResolution - 1) - startX + 1;
        height = Mathf.Clamp(centerZ + radiusSamples, 0, data.heightmapResolution - 1) - startZ + 1;
        return width > 0 && height > 0;
    }

    private static int GetStrokeSeed(DirectorIntentStroke stroke)
    {
        unchecked
        {
            int seed = EnvironmentRuntimeBootstrap.GetWorldSeed();
            seed = seed * 31 + Mathf.RoundToInt(stroke.Center.x * 7f);
            seed = seed * 31 + Mathf.RoundToInt(stroke.Center.z * 7f);
            seed = seed * 31 + (int)stroke.Intent * 97;
            return seed;
        }
    }

    private bool TryGetWorldPoint(out Vector3 point)
    {
        Camera camera = Camera.main;
        if (camera == null)
        {
            point = default;
            return false;
        }

        Ray ray = camera.ScreenPointToRay(Input.mousePosition);
        if (Physics.Raycast(ray, out RaycastHit hit, rayDistance))
        {
            point = hit.point;
            return true;
        }

        point = default;
        return false;
    }

    private static GameObject GetOrCreateDetailRoot()
    {
        GameObject root = GameObject.Find("Director Intent Details");
        if (root != null)
        {
            return root;
        }

        return new GameObject("Director Intent Details");
    }

    private static void RebuildDirectorNavMesh()
    {
        NavMeshSurface surface = FindFirstObjectByType<NavMeshSurface>();
        if (surface != null)
        {
            surface.BuildNavMesh();
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

    private static bool IsPointerOverUi()
    {
        return EventSystem.current != null && EventSystem.current.IsPointerOverGameObject();
    }

    private static void RefreshTerrainWindow()
    {
        if (DirectorTerrainWindow.Instance != null)
        {
            DirectorTerrainWindow.Instance.Refresh();
        }
    }

    private readonly struct DirectorIntentStroke
    {
        public DirectorIntentStroke(Vector3 center, float radius, float strength, DirectorIntentType intent)
        {
            Center = center;
            Radius = radius;
            Strength = strength;
            Intent = intent;
        }

        public readonly Vector3 Center;
        public readonly float Radius;
        public readonly float Strength;
        public readonly DirectorIntentType Intent;
    }
}
