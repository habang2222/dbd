using Unity.AI.Navigation;
using UnityEngine;
using UnityEngine.AI;
using UnityEngine.EventSystems;

public enum DirectorToolMode
{
    None,
    Delete,
    SpawnTree,
    SpawnStone,
    SpawnEnemy,
    RaiseTerrain,
    LowerTerrain,
    FlattenTerrain,
    CreateMountain,
    CreateCanyon,
    BrushMeadow,
    BrushForest,
    BrushMountain,
    BrushCanyon,
    BrushRiver,
    BrushWetland
}

public class DirectorWorldTool : MonoBehaviour
{
    public static DirectorWorldTool Instance { get; private set; }

    [SerializeField] private DirectorToolMode currentMode = DirectorToolMode.None;
    [SerializeField] private float rayDistance = 20000f;
    [SerializeField] private float terrainBrushRadius = 24f;
    [SerializeField] private float terrainBrushStrength = 9f;
    [SerializeField] private float intentBrushRadius = 80f;
    [SerializeField] private float intentBrushStrength = 1f;

    private Vector3 lastPaintPosition = new(float.PositiveInfinity, 0f, float.PositiveInfinity);
    private DirectorToolMode lastPaintMode = DirectorToolMode.None;

    public DirectorToolMode CurrentMode => currentMode;
    public float IntentBrushRadius => intentBrushRadius;
    public int PendingIntentCount => DirectorWorldPlanningService.PendingZoneCount;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<DirectorWorldTool>() != null)
        {
            return;
        }

        GameObject toolObject = new GameObject("Director World Tool");
        toolObject.AddComponent<DirectorWorldTool>();
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
        if (!SessionRoleService.IsDirector)
        {
            return;
        }

        HandleHotkeys();
        bool paintMode = IsIntentPaintMode(currentMode);
        bool wantsApply = paintMode ? Input.GetMouseButton(0) : Input.GetMouseButtonDown(0);
        if (currentMode == DirectorToolMode.None || !wantsApply || IsPointerOverUi())
        {
            return;
        }

        if (!TryGetWorldHit(out RaycastHit hit))
        {
            return;
        }

        ApplyTool(hit);
    }

    public void SetMode(DirectorToolMode mode)
    {
        currentMode = mode;
        lastPaintPosition = new Vector3(float.PositiveInfinity, 0f, float.PositiveInfinity);
        lastPaintMode = DirectorToolMode.None;
        RefreshDirectorUi();
    }

    public void AdjustIntentBrushRadius(float delta)
    {
        intentBrushRadius = Mathf.Clamp(intentBrushRadius + delta, 12f, 450f);
        RefreshDirectorUi();
    }

    public void SetIntentBrushRadius(float radius)
    {
        intentBrushRadius = Mathf.Clamp(radius, 12f, 450f);
        RefreshDirectorUi();
    }

    public void PushIntentZones()
    {
        if (!SessionRoleService.IsDirector)
        {
            return;
        }

        DirectorWorldPlanningService.ApplyPendingZones();
        RefreshDirectorUi();
    }

    public void FinishIntentEditing()
    {
        DirectorWorldPlanningService.FinishWorldSetup();
        SetMode(DirectorToolMode.None);
    }

    private void HandleHotkeys()
    {
        if (Input.GetKeyDown(KeyCode.Alpha0))
        {
            SetMode(DirectorToolMode.None);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha1))
        {
            SetMode(DirectorToolMode.Delete);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha2))
        {
            SetMode(DirectorToolMode.SpawnTree);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha3))
        {
            SetMode(DirectorToolMode.SpawnStone);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha4))
        {
            SetMode(DirectorToolMode.SpawnEnemy);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha5))
        {
            SetMode(DirectorToolMode.RaiseTerrain);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha6))
        {
            SetMode(DirectorToolMode.LowerTerrain);
        }
        else if (Input.GetKeyDown(KeyCode.Alpha7))
        {
            SetMode(DirectorToolMode.FlattenTerrain);
        }
    }

    private void ApplyTool(RaycastHit hit)
    {
        switch (currentMode)
        {
            case DirectorToolMode.Delete:
                DeleteTarget(hit.collider.gameObject);
                break;
            case DirectorToolMode.SpawnTree:
                SpawnTree(hit.point);
                break;
            case DirectorToolMode.SpawnStone:
                SpawnResource(hit.point, "stone_1", "Director Stone", 18f, new Color(0.42f, 0.43f, 0.45f, 1f), PrimitiveType.Cube, new Vector3(1.3f, 0.8f, 1.1f));
                break;
            case DirectorToolMode.SpawnEnemy:
                SpawnEnemy(hit.point);
                break;
            case DirectorToolMode.RaiseTerrain:
                EditTerrain(hit.point, 1f);
                break;
            case DirectorToolMode.LowerTerrain:
                EditTerrain(hit.point, -1f);
                break;
            case DirectorToolMode.FlattenTerrain:
                FlattenTerrain(hit.point);
                break;
            case DirectorToolMode.CreateMountain:
                CreateMountain(hit.point);
                break;
            case DirectorToolMode.CreateCanyon:
                CreateCanyon(hit.point);
                break;
            case DirectorToolMode.BrushMeadow:
                AddIntentZone(DirectorZoneType.Meadow, hit.point);
                break;
            case DirectorToolMode.BrushForest:
                AddIntentZone(DirectorZoneType.Forest, hit.point);
                break;
            case DirectorToolMode.BrushMountain:
                AddIntentZone(DirectorZoneType.Mountain, hit.point);
                break;
            case DirectorToolMode.BrushCanyon:
                AddIntentZone(DirectorZoneType.Canyon, hit.point);
                break;
            case DirectorToolMode.BrushRiver:
                AddIntentZone(DirectorZoneType.River, hit.point);
                break;
            case DirectorToolMode.BrushWetland:
                AddIntentZone(DirectorZoneType.Wetland, hit.point);
                break;
        }
    }

    private void AddIntentZone(DirectorZoneType type, Vector3 position)
    {
        float minSpacing = Mathf.Max(8f, intentBrushRadius * 0.45f);
        if (lastPaintMode == currentMode && Vector3.Distance(lastPaintPosition, position) < minSpacing)
        {
            return;
        }

        DirectorWorldPlanningService.PaintZone(type, position, intentBrushRadius, intentBrushStrength);
        lastPaintPosition = position;
        lastPaintMode = currentMode;
        RefreshDirectorUi();
    }

    private void DeleteTarget(GameObject target)
    {
        if (target == null || target.GetComponent<Terrain>() != null || target.GetComponentInParent<Canvas>() != null)
        {
            return;
        }

        if (target.name == "Ground" || target.name.Contains("Camera") || target.name.Contains("EventSystem"))
        {
            return;
        }

        Destroy(target.transform.root.gameObject);
    }

    private void SpawnTree(Vector3 position)
    {
        position = PlaceOnTerrain(position);
        GameObject treeRoot = new GameObject("Director Tree");
        treeRoot.transform.position = position;

        GameObject trunk = GameObject.CreatePrimitive(PrimitiveType.Cube);
        trunk.name = "Tree Trunk";
        trunk.transform.SetParent(treeRoot.transform, false);
        trunk.transform.localPosition = new Vector3(0f, 1.3f, 0f);
        trunk.transform.localScale = new Vector3(0.7f, 2.6f, 0.7f);
        trunk.GetComponent<Renderer>().material.color = new Color(0.36f, 0.20f, 0.09f, 1f);

        GameObject leaves = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        leaves.name = "Tree Crown";
        leaves.transform.SetParent(treeRoot.transform, false);
        leaves.transform.localPosition = new Vector3(0f, 3.0f, 0f);
        leaves.transform.localScale = new Vector3(2.5f, 1.5f, 2.5f);
        leaves.GetComponent<Renderer>().material.color = new Color(0.12f, 0.46f, 0.13f, 1f);
        Destroy(leaves.GetComponent<Collider>());

        BoxCollider collider = treeRoot.AddComponent<BoxCollider>();
        collider.center = new Vector3(0f, 1.5f, 0f);
        collider.size = new Vector3(2.4f, 3.2f, 2.4f);

        BranchResource resource = treeRoot.AddComponent<BranchResource>();
        resource.ConfigureYields("Director Tree", 8f, new[]
        {
            new PersonInventoryItem("wood_1", 2),
            new PersonInventoryItem("branch_1", 3),
            new PersonInventoryItem("leaf_1", 5)
        });
        AddNavIgnore(treeRoot);
    }

    private void SpawnResource(Vector3 position, string itemId, string displayName, float duration, Color color, PrimitiveType primitive, Vector3 scale)
    {
        GameObject resourceObject = GameObject.CreatePrimitive(primitive);
        resourceObject.name = displayName;
        resourceObject.transform.position = PlaceOnTerrain(position);
        resourceObject.transform.localScale = scale;
        resourceObject.GetComponent<Renderer>().material.color = color;
        BranchResource resource = resourceObject.AddComponent<BranchResource>();
        resource.Configure(itemId, displayName, duration);
        AddNavIgnore(resourceObject);
    }

    private void SpawnEnemy(Vector3 position)
    {
        GameObject enemyObject = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        enemyObject.transform.position = PlaceOnTerrain(position) + Vector3.up * 0.65f;
        enemyObject.transform.localScale = new Vector3(1.5f, 1.5f, 1.5f);
        enemyObject.GetComponent<Renderer>().material.color = new Color(0.85f, 0.12f, 0.12f, 1f);

        EnemyComponent enemy = enemyObject.AddComponent<EnemyComponent>();
        int index = FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None).Length + 1;
        enemy.Initialize($"director_enemy_{index}", $"Director Enemy {index}", 90f, 12f, 100f);

        EnemyWanderer wanderer = enemyObject.AddComponent<EnemyWanderer>();
        wanderer.Initialize(enemyObject.transform.position, 12f);
        enemyObject.AddComponent<UnitCombatController>();
        enemyObject.AddComponent<UnitDeathShrink>();
        AddNavIgnore(enemyObject);
    }

    private void EditTerrain(Vector3 worldPosition, float direction)
    {
        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return;
        }

        TerrainData data = terrain.terrainData;
        Vector3 local = worldPosition - terrain.transform.position;
        int centerX = Mathf.RoundToInt(local.x / data.size.x * (data.heightmapResolution - 1));
        int centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.heightmapResolution - 1));
        int radiusSamples = Mathf.Max(1, Mathf.RoundToInt(terrainBrushRadius / data.size.x * data.heightmapResolution));
        int startX = Mathf.Clamp(centerX - radiusSamples, 0, data.heightmapResolution - 1);
        int startZ = Mathf.Clamp(centerZ - radiusSamples, 0, data.heightmapResolution - 1);
        int width = Mathf.Clamp(centerX + radiusSamples, 0, data.heightmapResolution - 1) - startX + 1;
        int height = Mathf.Clamp(centerZ + radiusSamples, 0, data.heightmapResolution - 1) - startZ + 1;
        float[,] heights = data.GetHeights(startX, startZ, width, height);
        float delta = direction * terrainBrushStrength / data.size.y;

        for (int z = 0; z < height; z++)
        {
            for (int x = 0; x < width; x++)
            {
                float dx = (startX + x - centerX) / (float)radiusSamples;
                float dz = (startZ + z - centerZ) / (float)radiusSamples;
                float falloff = Mathf.Clamp01(1f - Mathf.Sqrt(dx * dx + dz * dz));
                heights[z, x] = Mathf.Clamp01(heights[z, x] + delta * falloff);
            }
        }

        data.SetHeights(startX, startZ, heights);
    }

    private void FlattenTerrain(Vector3 worldPosition)
    {
        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return;
        }

        TerrainData data = terrain.terrainData;
        Vector3 local = worldPosition - terrain.transform.position;
        int centerX = Mathf.RoundToInt(local.x / data.size.x * (data.heightmapResolution - 1));
        int centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.heightmapResolution - 1));
        int radiusSamples = Mathf.Max(1, Mathf.RoundToInt(terrainBrushRadius / data.size.x * data.heightmapResolution));
        int startX = Mathf.Clamp(centerX - radiusSamples, 0, data.heightmapResolution - 1);
        int startZ = Mathf.Clamp(centerZ - radiusSamples, 0, data.heightmapResolution - 1);
        int width = Mathf.Clamp(centerX + radiusSamples, 0, data.heightmapResolution - 1) - startX + 1;
        int height = Mathf.Clamp(centerZ + radiusSamples, 0, data.heightmapResolution - 1) - startZ + 1;
        float[,] heights = data.GetHeights(startX, startZ, width, height);
        float target = data.GetHeight(centerX, centerZ) / data.size.y;

        for (int z = 0; z < height; z++)
        {
            for (int x = 0; x < width; x++)
            {
                float dx = (startX + x - centerX) / (float)radiusSamples;
                float dz = (startZ + z - centerZ) / (float)radiusSamples;
                float falloff = Mathf.Clamp01(1f - Mathf.Sqrt(dx * dx + dz * dz));
                heights[z, x] = Mathf.Lerp(heights[z, x], target, falloff * 0.65f);
            }
        }

        data.SetHeights(startX, startZ, heights);
    }

    private void CreateMountain(Vector3 worldPosition)
    {
        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return;
        }

        TerrainData data = terrain.terrainData;
        Vector3 local = worldPosition - terrain.transform.position;
        int centerX = Mathf.RoundToInt(local.x / data.size.x * (data.heightmapResolution - 1));
        int centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.heightmapResolution - 1));
        int radiusSamples = Mathf.Max(2, Mathf.RoundToInt(95f / data.size.x * data.heightmapResolution));
        ApplyLandform(data, centerX, centerZ, radiusSamples, 58f / data.size.y, 0.42f, true);
    }

    private void CreateCanyon(Vector3 worldPosition)
    {
        Terrain terrain = Terrain.activeTerrain;
        if (terrain == null)
        {
            return;
        }

        TerrainData data = terrain.terrainData;
        Vector3 local = worldPosition - terrain.transform.position;
        int centerX = Mathf.RoundToInt(local.x / data.size.x * (data.heightmapResolution - 1));
        int centerZ = Mathf.RoundToInt(local.z / data.size.z * (data.heightmapResolution - 1));
        int radiusSamples = Mathf.Max(2, Mathf.RoundToInt(115f / data.size.x * data.heightmapResolution));
        ApplyLandform(data, centerX, centerZ, radiusSamples, -42f / data.size.y, 0.32f, false);
    }

    private static void ApplyLandform(TerrainData data, int centerX, int centerZ, int radiusSamples, float heightDelta, float rimDelta, bool peak)
    {
        int startX = Mathf.Clamp(centerX - radiusSamples, 0, data.heightmapResolution - 1);
        int startZ = Mathf.Clamp(centerZ - radiusSamples, 0, data.heightmapResolution - 1);
        int width = Mathf.Clamp(centerX + radiusSamples, 0, data.heightmapResolution - 1) - startX + 1;
        int height = Mathf.Clamp(centerZ + radiusSamples, 0, data.heightmapResolution - 1) - startZ + 1;
        float[,] heights = data.GetHeights(startX, startZ, width, height);

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

                float core = Mathf.Clamp01(1f - distance);
                float noise = Mathf.PerlinNoise((startX + x) * 0.11f, (startZ + z) * 0.11f);
                if (peak)
                {
                    float ridge = Mathf.Pow(core, 1.7f) * (0.78f + noise * 0.44f);
                    heights[z, x] = Mathf.Clamp01(heights[z, x] + heightDelta * ridge);
                }
                else
                {
                    float trench = Mathf.Pow(core, 0.75f) * (0.82f + noise * 0.22f);
                    float rim = Mathf.Clamp01(1f - Mathf.Abs(distance - 0.72f) / 0.16f) * rimDelta;
                    heights[z, x] = Mathf.Clamp01(heights[z, x] + heightDelta * trench + rim);
                }
            }
        }

        data.SetHeights(startX, startZ, heights);
    }

    private bool TryGetWorldHit(out RaycastHit hit)
    {
        Camera camera = Camera.main;
        if (camera == null)
        {
            hit = default;
            return false;
        }

        Ray ray = camera.ScreenPointToRay(Input.mousePosition);
        return Physics.Raycast(ray, out hit, rayDistance);
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

    private static bool IsPointerOverUi()
    {
        return EventSystem.current != null && EventSystem.current.IsPointerOverGameObject();
    }

    private static bool IsIntentPaintMode(DirectorToolMode mode)
    {
        return mode == DirectorToolMode.BrushMeadow
            || mode == DirectorToolMode.BrushForest
            || mode == DirectorToolMode.BrushMountain
            || mode == DirectorToolMode.BrushCanyon
            || mode == DirectorToolMode.BrushRiver
            || mode == DirectorToolMode.BrushWetland;
    }

    private static void RefreshDirectorUi()
    {
        DirectorToolHud hud = FindFirstObjectByType<DirectorToolHud>();
        if (hud != null)
        {
            hud.Refresh();
        }

        if (DirectorTerrainWindow.Instance != null)
        {
            DirectorTerrainWindow.Instance.Refresh();
        }
    }
}
