using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class WindowMenuPanel : MonoBehaviour
{
    public static WindowMenuPanel Instance { get; private set; }

    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<WindowMenuPanel>() != null)
        {
            return;
        }

        GameObject panelObject = new GameObject("Window Menu Panel");
        panelObject.AddComponent<WindowMenuPanel>();
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
        gameObject.SetActive(!gameObject.activeSelf);
    }

    public void Show()
    {
        gameObject.SetActive(true);
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Window Menu Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1215;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Window Menu");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(1f, 1f);
        panelRect.anchorMax = new Vector2(1f, 1f);
        panelRect.pivot = new Vector2(1f, 1f);
        panelRect.anchoredPosition = new Vector2(-24f, -70f);
        panelRect.sizeDelta = new Vector2(260f, 150f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.08f, 0.08f, 0.08f, 0.95f);

        RuntimeWindowControls controls = panel.AddComponent<RuntimeWindowControls>();

        Text title = CreateText(panel.transform, "\uBD80\uC18D \uCC3D", 22, TextAnchor.MiddleLeft);
        SetRect(title.rectTransform, 16f, -48f, -58f, -10f, 0f, 1f, 1f, 1f);

        Button mapButton = CreateButton(panel.transform, "\uB9F5");
        RectTransform mapButtonRect = mapButton.GetComponent<RectTransform>();
        mapButtonRect.anchorMin = new Vector2(0f, 1f);
        mapButtonRect.anchorMax = new Vector2(1f, 1f);
        mapButtonRect.offsetMin = new Vector2(18f, -108f);
        mapButtonRect.offsetMax = new Vector2(-18f, -62f);
        mapButton.onClick.AddListener(OpenMap);

        GameObject contentObject = new GameObject("Window Content");
        contentObject.transform.SetParent(panel.transform, false);
        RectTransform contentRoot = contentObject.AddComponent<RectTransform>();
        contentRoot.anchorMin = Vector2.zero;
        contentRoot.anchorMax = Vector2.one;
        contentRoot.offsetMin = new Vector2(18f, 18f);
        contentRoot.offsetMax = new Vector2(-18f, -58f);

        controls.Initialize(panelRect, contentRoot);
        controls.CreateButton("x", new Vector2(-8f, -10f), controls.Close);
    }

    private void OpenMap()
    {
        if (MapWindow.Instance == null)
        {
            GameObject mapWindow = new GameObject("Map Window");
            mapWindow.AddComponent<MapWindow>();
        }

        if (MapWindow.Instance != null)
        {
            MapWindow.Instance.Show();
        }
    }

    private Button CreateButton(Transform parent, string label)
    {
        GameObject buttonObject = new GameObject(label);
        buttonObject.transform.SetParent(parent, false);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.14f, 0.14f, 0.14f, 0.98f);

        Button button = buttonObject.AddComponent<Button>();

        Text text = CreateText(buttonObject.transform, label, 20, TextAnchor.MiddleCenter);
        text.rectTransform.anchorMin = Vector2.zero;
        text.rectTransform.anchorMax = Vector2.one;
        text.rectTransform.offsetMin = Vector2.zero;
        text.rectTransform.offsetMax = Vector2.zero;
        return button;
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
