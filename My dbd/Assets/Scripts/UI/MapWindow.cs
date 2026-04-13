using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;
using System.Collections.Generic;

public class MapWindow : MonoBehaviour
{
    private readonly List<Image> personMarkers = new();
    private readonly List<Image> enemyMarkers = new();
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
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
    }

    private void Update()
    {
        if (Time.unscaledTime < nextRefreshTime)
        {
            return;
        }

        nextRefreshTime = Time.unscaledTime + 0.15f;
        RefreshMap();
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Map Window Canvas").AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 998;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Map Window Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
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

        GameObject mapObject = new GameObject("Top Down Map Area");
        mapObject.transform.SetParent(contentRoot, false);
        mapArea = mapObject.AddComponent<RectTransform>();
        mapArea.anchorMin = Vector2.zero;
        mapArea.anchorMax = Vector2.one;
        mapArea.offsetMin = Vector2.zero;
        mapArea.offsetMax = Vector2.zero;

        Image mapBackground = mapObject.AddComponent<Image>();
        mapBackground.color = new Color(0.16f, 0.28f, 0.17f, 1f);

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

        PersonComponent[] people = FindObjectsByType<PersonComponent>(FindObjectsSortMode.None);
        EnemyComponent[] enemies = FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None);

        EnsureMarkerCount(personMarkers, people.Length, new Color(0.35f, 0.75f, 1f, 1f), "Person Marker");
        EnsureMarkerCount(enemyMarkers, enemies.Length, new Color(1f, 0.2f, 0.2f, 1f), "Enemy Marker");

        for (int i = 0; i < people.Length; i++)
        {
            PositionMarker(personMarkers[i], people[i].transform.position);
        }

        for (int i = people.Length; i < personMarkers.Count; i++)
        {
            personMarkers[i].gameObject.SetActive(false);
        }

        for (int i = 0; i < enemies.Length; i++)
        {
            PositionMarker(enemyMarkers[i], enemies[i].transform.position);
        }

        for (int i = enemies.Length; i < enemyMarkers.Count; i++)
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

    private Rect GetWorldRect()
    {
        GameObject ground = GameObject.Find("Ground");
        if (ground == null)
        {
            return new Rect(-45f, -30f, 90f, 60f);
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
