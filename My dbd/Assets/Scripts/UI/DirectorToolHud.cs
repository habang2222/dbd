using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class DirectorToolHud : MonoBehaviour
{
    private Text titleText;
    private Button[] buttons;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<DirectorToolHud>() != null)
        {
            return;
        }

        GameObject hudObject = new GameObject("Director Tool HUD");
        hudObject.AddComponent<DirectorToolHud>();
    }

    private void Awake()
    {
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        Refresh();
    }

    private void Update()
    {
        if (titleText != null)
        {
            titleText.transform.parent.gameObject.SetActive(SessionRoleService.IsDirector);
        }
    }

    public void Refresh()
    {
        if (titleText != null)
        {
            DirectorToolMode mode = DirectorWorldTool.Instance != null ? DirectorWorldTool.Instance.CurrentMode : DirectorToolMode.None;
            titleText.text = $"Director Tool: {mode}";
        }

        if (buttons == null || DirectorWorldTool.Instance == null)
        {
            return;
        }

        for (int i = 0; i < buttons.Length; i++)
        {
            DirectorToolMode mode = (DirectorToolMode)i;
            buttons[i].image.color = DirectorWorldTool.Instance.CurrentMode == mode
                ? new Color(0.34f, 0.24f, 0.12f, 0.98f)
                : new Color(0.12f, 0.12f, 0.12f, 0.96f);
        }
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Director Tool Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1290;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Director Tool Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(1f, 1f);
        panelRect.anchorMax = new Vector2(1f, 1f);
        panelRect.pivot = new Vector2(1f, 1f);
        panelRect.anchoredPosition = new Vector2(-18f, -126f);
        panelRect.sizeDelta = new Vector2(520f, 112f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.05f, 0.05f, 0.05f, 0.82f);

        titleText = CreateText(panel.transform, "Director Tool: None", 17, TextAnchor.MiddleLeft);
        RectTransform titleRect = titleText.rectTransform;
        titleRect.anchorMin = new Vector2(0f, 1f);
        titleRect.anchorMax = new Vector2(1f, 1f);
        titleRect.offsetMin = new Vector2(10f, -34f);
        titleRect.offsetMax = new Vector2(-10f, -6f);

        GameObject gridObject = new GameObject("Director Tool Buttons");
        gridObject.transform.SetParent(panel.transform, false);
        RectTransform gridRect = gridObject.AddComponent<RectTransform>();
        gridRect.anchorMin = Vector2.zero;
        gridRect.anchorMax = Vector2.one;
        gridRect.offsetMin = new Vector2(10f, 10f);
        gridRect.offsetMax = new Vector2(-10f, -38f);

        GridLayoutGroup grid = gridObject.AddComponent<GridLayoutGroup>();
        grid.padding = new RectOffset(10, 10, 38, 10);
        grid.cellSize = new Vector2(120f, 28f);
        grid.spacing = new Vector2(6f, 6f);

        buttons = new Button[8];
        buttons[0] = CreateToolButton(gridObject.transform, "0 None", DirectorToolMode.None);
        buttons[1] = CreateToolButton(gridObject.transform, "1 Delete", DirectorToolMode.Delete);
        buttons[2] = CreateToolButton(gridObject.transform, "2 Tree", DirectorToolMode.SpawnTree);
        buttons[3] = CreateToolButton(gridObject.transform, "3 Stone", DirectorToolMode.SpawnStone);
        buttons[4] = CreateToolButton(gridObject.transform, "4 Enemy", DirectorToolMode.SpawnEnemy);
        buttons[5] = CreateToolButton(gridObject.transform, "5 Raise", DirectorToolMode.RaiseTerrain);
        buttons[6] = CreateToolButton(gridObject.transform, "6 Lower", DirectorToolMode.LowerTerrain);
        buttons[7] = CreateToolButton(gridObject.transform, "7 Flatten", DirectorToolMode.FlattenTerrain);
        panel.SetActive(SessionRoleService.IsDirector);
    }

    private Button CreateToolButton(Transform parent, string label, DirectorToolMode mode)
    {
        GameObject buttonObject = new GameObject(label);
        buttonObject.transform.SetParent(parent, false);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.12f, 0.12f, 0.12f, 0.96f);
        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(() =>
        {
            if (DirectorWorldTool.Instance != null)
            {
                DirectorWorldTool.Instance.SetMode(mode);
            }
        });

        Text text = CreateText(buttonObject.transform, label, 14, TextAnchor.MiddleCenter);
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
