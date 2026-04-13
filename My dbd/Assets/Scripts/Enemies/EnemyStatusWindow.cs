using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class EnemyStatusWindow : MonoBehaviour
{
    public static EnemyStatusWindow Instance { get; private set; }

    private Text titleText;
    private Text bodyText;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<EnemyStatusWindow>() != null)
        {
            return;
        }

        GameObject windowObject = new GameObject("Enemy Status Window");
        windowObject.AddComponent<EnemyStatusWindow>();
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

    public void ShowEnemy(EnemyComponent enemy)
    {
        if (enemy == null)
        {
            return;
        }

        gameObject.SetActive(true);
        titleText.text = enemy.EnemyName;
        bodyText.text =
            "\uC801 \uC0C1\uD0DC\n" +
            $"\uCCB4\uB825: {enemy.Health:0}\n" +
            $"\uD798: {enemy.Strength:0}";
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Enemy Status Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1100;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Enemy Status Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(1f, 0f);
        panelRect.anchorMax = new Vector2(1f, 0f);
        panelRect.pivot = new Vector2(1f, 0f);
        panelRect.anchoredPosition = new Vector2(-24f, 24f);
        panelRect.sizeDelta = new Vector2(300f, 210f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.10f, 0.08f, 0.08f, 0.94f);

        RuntimeWindowControls controls = panel.AddComponent<RuntimeWindowControls>();

        titleText = CreateText(panel.transform, "\uC801", 24, TextAnchor.MiddleLeft);
        SetRect(titleText.rectTransform, 18f, -54f, -112f, -10f, 0f, 1f, 1f, 1f);

        GameObject contentObject = new GameObject("Window Content");
        contentObject.transform.SetParent(panel.transform, false);
        RectTransform contentRoot = contentObject.AddComponent<RectTransform>();
        contentRoot.anchorMin = Vector2.zero;
        contentRoot.anchorMax = Vector2.one;
        contentRoot.offsetMin = new Vector2(18f, 18f);
        contentRoot.offsetMax = new Vector2(-18f, -62f);

        bodyText = CreateText(contentRoot, string.Empty, 20, TextAnchor.UpperLeft);
        SetRect(bodyText.rectTransform, 0f, 0f, 0f, 0f, 0f, 0f, 1f, 1f);

        controls.Initialize(panelRect, contentRoot);
        controls.CreateButton("-", new Vector2(-76f, -10f), controls.ToggleMinimize);
        controls.CreateButton("\u25A1", new Vector2(-42f, -10f), controls.ToggleMaximize);
        controls.CreateButton("x", new Vector2(-8f, -10f), controls.Close);
        controls.CreateResizeHandle(new Vector2(220f, 150f));
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
