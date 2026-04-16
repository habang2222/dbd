using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class DirectorTerrainWindow : MonoBehaviour
{
    public static DirectorTerrainWindow Instance { get; private set; }

    private GameObject canvasObject;
    private Text statusText;
    private Slider brushSizeSlider;
    private Text brushSizeText;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<DirectorTerrainWindow>() != null)
        {
            return;
        }

        GameObject window = new GameObject("Director Terrain Window");
        window.AddComponent<DirectorTerrainWindow>();
    }

    private void Awake()
    {
        Instance = this;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        Hide();
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    public void Show()
    {
        if (!SessionRoleService.IsDirector)
        {
            Hide();
            return;
        }

        canvasObject.SetActive(true);
    }

    public void Hide()
    {
        if (canvasObject != null)
        {
            canvasObject.SetActive(false);
        }
    }

    public void Refresh()
    {
        if (statusText == null)
        {
            return;
        }

        DirectorIntentBrushSystem brush = DirectorIntentBrushSystem.Instance;
        if (brush == null)
        {
            statusText.text = "붓 시스템 준비 중";
            return;
        }

        statusText.text = $"붓: {GetIntentLabel(brush.CurrentIntent)} | 크기: {Mathf.RoundToInt(brush.BrushRadius)} | 대기: {brush.PendingStrokeCount}";
        if (brushSizeText != null)
        {
            brushSizeText.text = $"붓 크기 {Mathf.RoundToInt(brush.BrushRadius)}";
        }

        if (brushSizeSlider != null)
        {
            brushSizeSlider.SetValueWithoutNotify(brush.BrushRadius);
        }
    }

    private void CreateUi()
    {
        canvasObject = new GameObject("Director Terrain Canvas");
        canvasObject.transform.SetParent(transform, false);
        Canvas canvas = canvasObject.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1310;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = CreatePanel(canvas.transform, "지형 붓", new Vector2(-24f, -250f), new Vector2(360f, 560f));
        statusText = CreateText(panel.transform, "붓 시스템 준비 중", 17, TextAnchor.MiddleLeft);
        statusText.rectTransform.anchorMin = new Vector2(0f, 1f);
        statusText.rectTransform.anchorMax = new Vector2(1f, 1f);
        statusText.rectTransform.offsetMin = new Vector2(18f, -86f);
        statusText.rectTransform.offsetMax = new Vector2(-18f, -54f);

        CreateButton(panel.transform, "들판 붓", new Vector2(18f, -130f), () => SelectBrush(DirectorIntentType.Meadow));
        CreateButton(panel.transform, "숲 붓", new Vector2(188f, -130f), () => SelectBrush(DirectorIntentType.Forest));
        CreateButton(panel.transform, "산 붓", new Vector2(18f, -184f), () => SelectBrush(DirectorIntentType.Mountain));
        CreateButton(panel.transform, "협곡 붓", new Vector2(188f, -184f), () => SelectBrush(DirectorIntentType.Canyon));
        CreateButton(panel.transform, "강 붓", new Vector2(18f, -238f), () => SelectBrush(DirectorIntentType.River));
        CreateButton(panel.transform, "습지 붓", new Vector2(188f, -238f), () => SelectBrush(DirectorIntentType.Wetland));

        CreateButton(panel.transform, "붓 작게", new Vector2(18f, -306f), () => ChangeBrushSize(-20f));
        CreateButton(panel.transform, "붓 크게", new Vector2(188f, -306f), () => ChangeBrushSize(20f));
        CreateBrushSizeSlider(panel.transform, new Vector2(18f, -362f));
        CreateButton(panel.transform, "Push 적용", new Vector2(18f, -430f), Push);
        CreateButton(panel.transform, "Finish 완료", new Vector2(188f, -430f), Finish);
        CreateButton(panel.transform, "대기 지우기", new Vector2(18f, -494f), ClearPending);
        Refresh();
    }

    private GameObject CreatePanel(Transform parent, string title, Vector2 position, Vector2 size)
    {
        GameObject panel = new GameObject("Director Terrain Panel");
        panel.transform.SetParent(parent, false);
        RectTransform rect = panel.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(1f, 1f);
        rect.anchorMax = new Vector2(1f, 1f);
        rect.pivot = new Vector2(1f, 1f);
        rect.anchoredPosition = position;
        rect.sizeDelta = size;
        panel.AddComponent<Image>().color = new Color(0.06f, 0.06f, 0.06f, 0.94f);

        Text titleText = CreateText(panel.transform, title, 24, TextAnchor.MiddleLeft);
        titleText.rectTransform.anchorMin = new Vector2(0f, 1f);
        titleText.rectTransform.anchorMax = new Vector2(1f, 1f);
        titleText.rectTransform.offsetMin = new Vector2(18f, -52f);
        titleText.rectTransform.offsetMax = new Vector2(-18f, -10f);
        return panel;
    }

    private void CreateButton(Transform parent, string label, Vector2 position, UnityEngine.Events.UnityAction action)
    {
        GameObject buttonObject = new GameObject(label);
        buttonObject.transform.SetParent(parent, false);
        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(0f, 1f);
        rect.offsetMin = new Vector2(position.x, position.y - 42f);
        rect.offsetMax = new Vector2(position.x + 152f, position.y);
        buttonObject.AddComponent<Image>().color = new Color(0.14f, 0.14f, 0.14f, 0.98f);
        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(action);
        Text text = CreateText(buttonObject.transform, label, 20, TextAnchor.MiddleCenter);
        text.rectTransform.anchorMin = Vector2.zero;
        text.rectTransform.anchorMax = Vector2.one;
    }

    private void CreateBrushSizeSlider(Transform parent, Vector2 position)
    {
        brushSizeText = CreateText(parent, "붓 크기 70", 18, TextAnchor.MiddleLeft);
        brushSizeText.rectTransform.anchorMin = new Vector2(0f, 1f);
        brushSizeText.rectTransform.anchorMax = new Vector2(0f, 1f);
        brushSizeText.rectTransform.offsetMin = new Vector2(position.x, position.y - 26f);
        brushSizeText.rectTransform.offsetMax = new Vector2(position.x + 324f, position.y);

        GameObject sliderObject = new GameObject("Brush Size Slider");
        sliderObject.transform.SetParent(parent, false);
        RectTransform rect = sliderObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(0f, 1f);
        rect.offsetMin = new Vector2(position.x, position.y - 58f);
        rect.offsetMax = new Vector2(position.x + 324f, position.y - 34f);

        GameObject background = new GameObject("Background");
        background.transform.SetParent(sliderObject.transform, false);
        RectTransform backgroundRect = background.AddComponent<RectTransform>();
        backgroundRect.anchorMin = new Vector2(0f, 0.25f);
        backgroundRect.anchorMax = new Vector2(1f, 0.75f);
        backgroundRect.offsetMin = Vector2.zero;
        backgroundRect.offsetMax = Vector2.zero;
        background.AddComponent<Image>().color = new Color(0.10f, 0.10f, 0.10f, 1f);

        GameObject fillArea = new GameObject("Fill Area");
        fillArea.transform.SetParent(sliderObject.transform, false);
        RectTransform fillAreaRect = fillArea.AddComponent<RectTransform>();
        fillAreaRect.anchorMin = new Vector2(0f, 0.25f);
        fillAreaRect.anchorMax = new Vector2(1f, 0.75f);
        fillAreaRect.offsetMin = new Vector2(4f, 0f);
        fillAreaRect.offsetMax = new Vector2(-4f, 0f);

        GameObject fill = new GameObject("Fill");
        fill.transform.SetParent(fillArea.transform, false);
        RectTransform fillRect = fill.AddComponent<RectTransform>();
        fillRect.anchorMin = Vector2.zero;
        fillRect.anchorMax = Vector2.one;
        fillRect.offsetMin = Vector2.zero;
        fillRect.offsetMax = Vector2.zero;
        fill.AddComponent<Image>().color = new Color(0.25f, 0.58f, 0.24f, 1f);

        GameObject handleArea = new GameObject("Handle Slide Area");
        handleArea.transform.SetParent(sliderObject.transform, false);
        RectTransform handleAreaRect = handleArea.AddComponent<RectTransform>();
        handleAreaRect.anchorMin = Vector2.zero;
        handleAreaRect.anchorMax = Vector2.one;
        handleAreaRect.offsetMin = new Vector2(8f, 0f);
        handleAreaRect.offsetMax = new Vector2(-8f, 0f);

        GameObject handle = new GameObject("Handle");
        handle.transform.SetParent(handleArea.transform, false);
        RectTransform handleRect = handle.AddComponent<RectTransform>();
        handleRect.sizeDelta = new Vector2(22f, 22f);
        handle.AddComponent<Image>().color = new Color(0.88f, 0.88f, 0.88f, 1f);

        brushSizeSlider = sliderObject.AddComponent<Slider>();
        brushSizeSlider.minValue = 18f;
        brushSizeSlider.maxValue = 220f;
        brushSizeSlider.wholeNumbers = true;
        brushSizeSlider.targetGraphic = handle.GetComponent<Image>();
        brushSizeSlider.fillRect = fillRect;
        brushSizeSlider.handleRect = handleRect;
        brushSizeSlider.value = 70f;
        brushSizeSlider.onValueChanged.AddListener(SetBrushSize);
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
        text.text = value;
        return text;
    }

    private static void SelectBrush(DirectorIntentType intent)
    {
        if (DirectorIntentBrushSystem.Instance != null)
        {
            DirectorIntentBrushSystem.Instance.SelectBrush(intent);
        }
    }

    private static void ChangeBrushSize(float amount)
    {
        if (DirectorIntentBrushSystem.Instance != null)
        {
            DirectorIntentBrushSystem.Instance.AddBrushRadius(amount);
        }
    }

    private static void SetBrushSize(float radius)
    {
        if (DirectorIntentBrushSystem.Instance != null)
        {
            DirectorIntentBrushSystem.Instance.SetBrushRadius(radius);
        }
    }

    private static void Push()
    {
        if (DirectorIntentBrushSystem.Instance != null)
        {
            DirectorIntentBrushSystem.Instance.Push();
        }
    }

    private static void Finish()
    {
        if (DirectorIntentBrushSystem.Instance != null)
        {
            DirectorIntentBrushSystem.Instance.Finish();
        }
    }

    private static void ClearPending()
    {
        if (DirectorIntentBrushSystem.Instance != null)
        {
            DirectorIntentBrushSystem.Instance.ClearPending();
        }
    }

    private static string GetIntentLabel(DirectorIntentType intent)
    {
        switch (intent)
        {
            case DirectorIntentType.Forest:
                return "숲";
            case DirectorIntentType.Mountain:
                return "산";
            case DirectorIntentType.Canyon:
                return "협곡";
            case DirectorIntentType.River:
                return "강";
            case DirectorIntentType.Wetland:
                return "습지";
            default:
                return "들판";
        }
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
