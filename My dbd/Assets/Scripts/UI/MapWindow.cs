using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;
using System.Collections.Generic;

public class MapWindow : MonoBehaviour
{
    public static MapWindow Instance { get; private set; }

    private readonly List<Image> personMarkers = new();
    private readonly List<Image> enemyMarkers = new();
    private readonly List<Image> terrainMarkers = new();
    private RectTransform cameraViewMarker;
    private Canvas canvas;
    private RectTransform windowRoot;
    private RectTransform mapArea;
    private Font font;
    private float nextRefreshTime;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<MapWindow>() != null)
        {
            return;
        }

        GameObject mapWindowObject = new GameObject("Map Window");
        mapWindowObject.AddComponent<MapWindow>();
    }

    private void Awake()
    {
        Instance = this;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        gameObject.SetActive(false);
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    public void Toggle()
    {
        bool shouldShow = windowRoot == null || !windowRoot.gameObject.activeSelf;
        gameObject.SetActive(true);
        if (windowRoot != null)
        {
            windowRoot.gameObject.SetActive(shouldShow);
        }

        if (shouldShow)
        {
            RefreshMap();
        }
    }

    public void Show()
    {
        gameObject.SetActive(true);
        if (windowRoot != null)
        {
            windowRoot.gameObject.SetActive(true);
        }

        RefreshMap();
    }

    private void Update()
    {
        if (windowRoot != null && !windowRoot.gameObject.activeSelf)
        {
            return;
        }

        if (Time.unscaledTime < nextRefreshTime)
        {
            return;
        }

        nextRefreshTime = Time.unscaledTime + 0.15f;
        RefreshMap();
    }

    private void CreateUi()
    {
        canvas = new GameObject("Map Window Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 998;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Map Window Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        windowRoot = panelRect;
        panelRect.anchorMin = new Vector2(1f, 1f);
        panelRect.anchorMax = new Vector2(1f, 1f);
        panelRect.pivot = new Vector2(1f, 1f);
        panelRect.anchoredPosition = new Vector2(-24f, -82f);
        panelRect.sizeDelta = new Vector2(430f, 360f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.09f, 0.09f, 0.09f, 0.94f);

        RuntimeWindowControls controls = panel.AddComponent<RuntimeWindowControls>();

        Text title = CreateText(panel.transform, "\uB9F5", 24, TextAnchor.MiddleLeft);
        SetRect(title.rectTransform, 18f, -54f, -112f, -10f, 0f, 1f, 1f, 1f);

        GameObject contentObject = new GameObject("Window Content");
        contentObject.transform.SetParent(panel.transform, false);
        RectTransform contentRoot = contentObject.AddComponent<RectTransform>();
        contentRoot.anchorMin = Vector2.zero;
        contentRoot.anchorMax = Vector2.one;
        contentRoot.offsetMin = new Vector2(18f, 18f);
        contentRoot.offsetMax = new Vector2(-18f, -62f);
        contentObject.AddComponent<RectMask2D>();

        GameObject mapObject = new GameObject("Top Down Map Area");
        mapObject.transform.SetParent(contentRoot, false);
        mapArea = mapObject.AddComponent<RectTransform>();
        mapArea.anchorMin = Vector2.zero;
        mapArea.anchorMax = Vector2.one;
        mapArea.offsetMin = Vector2.zero;
        mapArea.offsetMax = Vector2.zero;

        Image mapBackground = mapObject.AddComponent<Image>();
        mapBackground.color = new Color(0.24f, 0.48f, 0.20f, 1f);
        CreateCameraViewMarker();

        controls.Initialize(panelRect, contentRoot);
        controls.CreateButton("-", new Vector2(-76f, -10f), controls.ToggleMinimize);
        controls.CreateButton("\u25A1", new Vector2(-42f, -10f), controls.ToggleMaximize);
        controls.CreateButton("x", new Vector2(-8f, -10f), controls.Close);
        controls.CreateResizeHandle(new Vector2(240f, 180f));
    }

    private void RefreshMap()
    {
        if (mapArea == null)
        {
            return;
        }

        Canvas.ForceUpdateCanvases();
        List<PersonComponent> visiblePeople = new();
        RefreshTerrainMarkers();
        foreach (PersonComponent person in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (WorldVisibilityService.CanLocalPlayerSeePerson(person))
            {
                visiblePeople.Add(person);
            }
        }

        List<EnemyComponent> visibleEnemies = new();
        foreach (EnemyComponent enemy in FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None))
        {
            if (WorldVisibilityService.CanLocalPlayerSeeEnemy(enemy))
            {
                visibleEnemies.Add(enemy);
            }
        }

        EnsureMarkerCount(personMarkers, visiblePeople.Count, new Color(0.35f, 0.75f, 1f, 1f), "Person Marker");
        EnsureMarkerCount(enemyMarkers, visibleEnemies.Count, new Color(1f, 0.2f, 0.2f, 1f), "Enemy Marker");
        UpdateCameraViewMarker();

        for (int i = 0; i < visiblePeople.Count; i++)
        {
            PositionMarker(personMarkers[i], visiblePeople[i].transform.position);
        }

        for (int i = visiblePeople.Count; i < personMarkers.Count; i++)
        {
            personMarkers[i].gameObject.SetActive(false);
        }

        for (int i = 0; i < visibleEnemies.Count; i++)
        {
            PositionMarker(enemyMarkers[i], visibleEnemies[i].transform.position);
        }

        for (int i = visibleEnemies.Count; i < enemyMarkers.Count; i++)
        {
            enemyMarkers[i].gameObject.SetActive(false);
        }
    }

    private void PositionMarker(Image marker, Vector3 worldPosition)
    {
        marker.gameObject.SetActive(true);

        Rect worldRect = GetWorldRect();
        float normalizedX = Mathf.InverseLerp(worldRect.xMin, worldRect.xMax, worldPosition.x);
        float normalizedY = Mathf.InverseLerp(worldRect.yMin, worldRect.yMax, worldPosition.z);

        RectTransform rect = marker.rectTransform;
        rect.anchorMin = new Vector2(normalizedX, normalizedY);
        rect.anchorMax = new Vector2(normalizedX, normalizedY);
        rect.anchoredPosition = Vector2.zero;
    }

    private void RefreshTerrainMarkers()
    {
        foreach (Image marker in terrainMarkers)
        {
            if (marker != null)
            {
                Destroy(marker.gameObject);
            }
        }

        terrainMarkers.Clear();

        foreach (Transform zone in FindObjectsByType<Transform>(FindObjectsSortMode.None))
        {
            if (zone.name == "Dirt Zone")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.24f, 0.48f, 0.20f, 1f), "Map Dirt Zone");
            }
            else if (zone.name == "Sand Zone")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.78f, 0.58f, 0.25f, 1f), "Map Sand Zone");
            }
            else if (zone.name == "River")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.12f, 0.35f, 0.90f, 0.9f), "Map River");
            }
            else if (zone.name == "Wide River")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.10f, 0.32f, 0.78f, 0.9f), "Map Wide River");
            }
            else if (zone.name == "Narrow River")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.16f, 0.45f, 0.95f, 0.9f), "Map Narrow River");
            }
            else if (zone.name == "Waterfall")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.70f, 0.88f, 1f, 0.9f), "Map Waterfall");
            }
            else if (zone.name == "Pond" || zone.name == "Lake")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.08f, 0.34f, 0.72f, 0.85f), "Map Fresh Water");
            }
            else if (zone.name == "Wetland" || zone.name == "Marsh")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.13f, 0.31f, 0.20f, 0.9f), "Map Wetland");
            }
            else if (zone.name == "Mountain" || zone.name == "High Mountain" || zone.name == "Great Mountain")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.33f, 0.36f, 0.34f, 0.9f), "Map Mountain");
            }
            else if (zone.name == "Hill" || zone.name == "Plateau")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.32f, 0.50f, 0.24f, 0.9f), "Map Highlands");
            }
            else if (zone.name == "Canyon" || zone.name == "Cliff" || zone.name == "Crater")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.50f, 0.26f, 0.17f, 0.9f), "Map Rock Formation");
            }
            else if (zone.name == "Cave Mouth" || zone.name == "Cave" || zone.name == "Limestone Cave" || zone.name == "Tunnel")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.08f, 0.08f, 0.09f, 0.9f), "Map Cave");
            }
            else if (zone.name == "Valley" || zone.name == "Basin" || zone.name == "Meadow" || zone.name == "Clay Bank")
            {
                CreateTerrainMarker(zone.transform.position, zone.transform.localScale, new Color(0.29f, 0.45f, 0.22f, 0.9f), "Map Lowland");
            }
        }
    }

    private void CreateTerrainMarker(Vector3 worldPosition, Vector3 worldScale, Color color, string markerName)
    {
        GameObject markerObject = new GameObject(markerName);
        markerObject.transform.SetParent(mapArea, false);
        RectTransform rect = markerObject.AddComponent<RectTransform>();
        Image image = markerObject.AddComponent<Image>();
        image.color = color;
        markerObject.transform.SetAsFirstSibling();
        terrainMarkers.Add(image);

        Rect worldRect = GetWorldRect();
        float normalizedX = Mathf.InverseLerp(worldRect.xMin, worldRect.xMax, worldPosition.x);
        float normalizedY = Mathf.InverseLerp(worldRect.yMin, worldRect.yMax, worldPosition.z);
        rect.anchorMin = new Vector2(normalizedX, normalizedY);
        rect.anchorMax = new Vector2(normalizedX, normalizedY);
        rect.anchoredPosition = Vector2.zero;
        rect.sizeDelta = new Vector2(
            Mathf.Max(4f, mapArea.rect.width * worldScale.x / Mathf.Max(1f, worldRect.width)),
            Mathf.Max(4f, mapArea.rect.height * worldScale.z / Mathf.Max(1f, worldRect.height)));
    }

    private void CreateCameraViewMarker()
    {
        GameObject markerObject = new GameObject("Camera View Marker");
        markerObject.transform.SetParent(mapArea, false);
        cameraViewMarker = markerObject.AddComponent<RectTransform>();
        Image image = markerObject.AddComponent<Image>();
        image.color = new Color(1f, 1f, 1f, 0.08f);
        Outline outline = markerObject.AddComponent<Outline>();
        outline.effectColor = new Color(1f, 1f, 1f, 0.95f);
        outline.effectDistance = new Vector2(2f, 2f);
        markerObject.SetActive(false);
    }

    private void UpdateCameraViewMarker()
    {
        if (cameraViewMarker == null)
        {
            return;
        }

        Camera camera = Camera.main;
        if (camera == null)
        {
            cameraViewMarker.gameObject.SetActive(false);
            return;
        }

        if (!TryGetGroundPoint(camera, new Vector3(0f, 0f, 0f), out Vector3 bottomLeft)
            || !TryGetGroundPoint(camera, new Vector3(1f, 0f, 0f), out Vector3 bottomRight)
            || !TryGetGroundPoint(camera, new Vector3(1f, 1f, 0f), out Vector3 topRight)
            || !TryGetGroundPoint(camera, new Vector3(0f, 1f, 0f), out Vector3 topLeft))
        {
            cameraViewMarker.gameObject.SetActive(false);
            return;
        }

        Rect worldRect = GetWorldRect();
        Vector2 a = WorldToMapNormalized(bottomLeft, worldRect);
        Vector2 b = WorldToMapNormalized(bottomRight, worldRect);
        Vector2 c = WorldToMapNormalized(topRight, worldRect);
        Vector2 d = WorldToMapNormalized(topLeft, worldRect);

        float minX = Mathf.Clamp01(Mathf.Min(a.x, b.x, c.x, d.x));
        float maxX = Mathf.Clamp01(Mathf.Max(a.x, b.x, c.x, d.x));
        float minY = Mathf.Clamp01(Mathf.Min(a.y, b.y, c.y, d.y));
        float maxY = Mathf.Clamp01(Mathf.Max(a.y, b.y, c.y, d.y));

        cameraViewMarker.anchorMin = new Vector2(minX, minY);
        cameraViewMarker.anchorMax = new Vector2(maxX, maxY);
        cameraViewMarker.offsetMin = Vector2.zero;
        cameraViewMarker.offsetMax = Vector2.zero;
        cameraViewMarker.SetAsLastSibling();
        cameraViewMarker.gameObject.SetActive(true);
    }

    private static bool TryGetGroundPoint(Camera camera, Vector3 viewportPoint, out Vector3 groundPoint)
    {
        Ray ray = camera.ViewportPointToRay(viewportPoint);
        Plane groundPlane = new(Vector3.up, Vector3.zero);
        if (groundPlane.Raycast(ray, out float distance))
        {
            groundPoint = ray.GetPoint(distance);
            return true;
        }

        groundPoint = Vector3.zero;
        return false;
    }

    private static Vector2 WorldToMapNormalized(Vector3 worldPosition, Rect worldRect)
    {
        return new Vector2(
            Mathf.InverseLerp(worldRect.xMin, worldRect.xMax, worldPosition.x),
            Mathf.InverseLerp(worldRect.yMin, worldRect.yMax, worldPosition.z));
    }

    private Rect GetWorldRect()
    {
        GameObject ground = GameObject.Find("Ground");
        Terrain terrain = ground != null ? ground.GetComponent<Terrain>() : Terrain.activeTerrain;
        if (terrain != null)
        {
            Vector3 terrainCenter = terrain.transform.position + terrain.terrainData.size * 0.5f;
            Vector3 size = terrain.terrainData.size;
            return new Rect(terrainCenter.x - (size.x * 0.5f), terrainCenter.z - (size.z * 0.5f), size.x, size.z);
        }

        if (ground == null)
        {
            float half = EnvironmentRuntimeBootstrap.WorldSize * 0.5f;
            return new Rect(-half, -half, EnvironmentRuntimeBootstrap.WorldSize, EnvironmentRuntimeBootstrap.WorldSize);
        }

        Vector3 center = ground.transform.position;
        Vector3 scale = ground.transform.localScale;
        return new Rect(center.x - (scale.x * 0.5f), center.z - (scale.z * 0.5f), scale.x, scale.z);
    }

    private void EnsureMarkerCount(List<Image> markers, int count, Color color, string markerName)
    {
        while (markers.Count < count)
        {
            GameObject markerObject = new GameObject(markerName);
            markerObject.transform.SetParent(mapArea, false);

            RectTransform rect = markerObject.AddComponent<RectTransform>();
            rect.sizeDelta = new Vector2(10f, 10f);

            Image image = markerObject.AddComponent<Image>();
            image.color = color;
            markers.Add(image);
        }

        foreach (Image marker in markers)
        {
            if (marker != null)
            {
                marker.transform.SetAsLastSibling();
            }
        }
    }

    private Text CreateText(Transform parent, string value, int size, TextAnchor alignment)
    {
        GameObject textObject = new GameObject("Text");
        textObject.transform.SetParent(parent, false);

        RectTransform rect = textObject.AddComponent<RectTransform>();
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.offsetMin = Vector2.zero;
        rect.offsetMax = Vector2.zero;

        Text text = textObject.AddComponent<Text>();
        text.font = font;
        text.fontSize = size;
        text.alignment = alignment;
        text.color = Color.white;
        text.horizontalOverflow = HorizontalWrapMode.Wrap;
        text.verticalOverflow = VerticalWrapMode.Truncate;
        text.text = value;
        return text;
    }

    private static void SetRect(RectTransform rect, float left, float bottom, float right, float top, float anchorMinX, float anchorMinY, float anchorMaxX, float anchorMaxY)
    {
        rect.anchorMin = new Vector2(anchorMinX, anchorMinY);
        rect.anchorMax = new Vector2(anchorMaxX, anchorMaxY);
        rect.offsetMin = new Vector2(left, bottom);
        rect.offsetMax = new Vector2(right, top);
    }

    private static void EnsureEventSystem()
    {
        if (FindFirstObjectByType<EventSystem>() != null)
        {
            return;
        }

        GameObject eventSystemObject = new GameObject("EventSystem");
        eventSystemObject.AddComponent<EventSystem>();
        eventSystemObject.AddComponent<StandaloneInputModule>();
    }
}
