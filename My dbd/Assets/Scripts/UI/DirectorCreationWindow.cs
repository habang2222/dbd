using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class DirectorCreationWindow : MonoBehaviour
{
    public static DirectorCreationWindow Instance { get; private set; }

    private GameObject canvasObject;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<DirectorCreationWindow>() != null)
        {
            return;
        }

        GameObject window = new GameObject("Director Creation Window");
        window.AddComponent<DirectorCreationWindow>();
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

    private void CreateUi()
    {
        canvasObject = new GameObject("Director Creation Canvas");
        canvasObject.transform.SetParent(transform, false);
        Canvas canvas = canvasObject.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1310;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = CreatePanel(canvas.transform, "생성", new Vector2(-24f, -250f), new Vector2(260f, 190f));
        CreateButton(panel.transform, "몹 생성", new Vector2(18f, -78f), () => SetMode(DirectorToolMode.SpawnEnemy));
        CreateButton(panel.transform, "재료 생성", new Vector2(18f, -132f), () => SetMode(DirectorToolMode.SpawnStone));
    }

    private GameObject CreatePanel(Transform parent, string title, Vector2 position, Vector2 size)
    {
        GameObject panel = new GameObject("Director Creation Panel");
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
        rect.anchorMax = new Vector2(1f, 1f);
        rect.offsetMin = new Vector2(position.x, position.y - 42f);
        rect.offsetMax = new Vector2(-18f, position.y);
        buttonObject.AddComponent<Image>().color = new Color(0.14f, 0.14f, 0.14f, 0.98f);
        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(action);
        Text text = CreateText(buttonObject.transform, label, 20, TextAnchor.MiddleCenter);
        text.rectTransform.anchorMin = Vector2.zero;
        text.rectTransform.anchorMax = Vector2.one;
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

    private static void SetMode(DirectorToolMode mode)
    {
        if (DirectorWorldTool.Instance != null)
        {
            DirectorWorldTool.Instance.SetMode(mode);
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
