using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class OwnedItemsWindow : MonoBehaviour
{
    public static OwnedItemsWindow Instance { get; private set; }

    private readonly List<Text[]> rows = new();
    private readonly List<OwnedItemRowData> visibleItems = new();
    private RectTransform listRoot;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<OwnedItemsWindow>() != null)
        {
            return;
        }

        GameObject windowObject = new GameObject("Owned Items Window");
        windowObject.AddComponent<OwnedItemsWindow>();
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
        if (gameObject.activeSelf)
        {
            Refresh();
        }
    }

    public void Refresh()
    {
        visibleItems.Clear();
        foreach (PersonComponent person in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            foreach (PersonInventoryItem item in person.Inventory.items)
            {
                if (item == null || string.IsNullOrWhiteSpace(item.itemId) || item.count <= 0)
                {
                    continue;
                }

                visibleItems.Add(new OwnedItemRowData(person.PersonName, item.itemId, item.count));
            }
        }

        visibleItems.Sort((left, right) =>
        {
            int ownerCompare = string.Compare(left.OwnerName, right.OwnerName, System.StringComparison.Ordinal);
            return ownerCompare != 0
                ? ownerCompare
                : string.Compare(left.ItemId, right.ItemId, System.StringComparison.Ordinal);
        });

        EnsureRowCount(visibleItems.Count);
        for (int index = 0; index < visibleItems.Count; index++)
        {
            OwnedItemRowData entry = visibleItems[index];
            Text[] cells = rows[index];
            cells[0].text = entry.OwnerName;
            cells[1].text = GetDisplayName(entry.ItemId);
            cells[2].text = GetCategory(entry.ItemId);
            cells[3].text = entry.Count.ToString();
            cells[0].transform.parent.gameObject.SetActive(true);
        }

        for (int i = visibleItems.Count; i < rows.Count; i++)
        {
            rows[i][0].transform.parent.gameObject.SetActive(false);
        }
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Owned Items Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1120;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Owned Items Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(0.5f, 0.5f);
        panelRect.anchorMax = new Vector2(0.5f, 0.5f);
        panelRect.pivot = new Vector2(0.5f, 0.5f);
        panelRect.anchoredPosition = Vector2.zero;
        panelRect.sizeDelta = new Vector2(560f, 320f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.08f, 0.08f, 0.08f, 0.94f);

        RuntimeWindowControls controls = panel.AddComponent<RuntimeWindowControls>();

        Text title = CreateText(panel.transform, "\uC18C\uC720\uD55C \uBB3C\uAC74", 24, TextAnchor.MiddleLeft);
        SetRect(title.rectTransform, 18f, -54f, -112f, -10f, 0f, 1f, 1f, 1f);

        GameObject contentObject = new GameObject("Window Content");
        contentObject.transform.SetParent(panel.transform, false);
        RectTransform contentRoot = contentObject.AddComponent<RectTransform>();
        contentRoot.anchorMin = Vector2.zero;
        contentRoot.anchorMax = Vector2.one;
        contentRoot.offsetMin = new Vector2(18f, 18f);
        contentRoot.offsetMax = new Vector2(-18f, -62f);
        contentObject.AddComponent<RectMask2D>();

        CreateHeader(contentRoot);

        RectTransform listPanel = CreatePanel("Owned Item Scroll", contentRoot, new Color(0f, 0f, 0f, 0.01f));
        listPanel.anchorMin = Vector2.zero;
        listPanel.anchorMax = Vector2.one;
        listPanel.offsetMin = Vector2.zero;
        listPanel.offsetMax = new Vector2(0f, -34f);
        ScrollRect scrollRect = listPanel.gameObject.AddComponent<ScrollRect>();
        scrollRect.horizontal = false;
        scrollRect.scrollSensitivity = 85f;

        RectTransform viewport = CreatePanel("Viewport", listPanel, new Color(0f, 0f, 0f, 0.01f));
        viewport.anchorMin = Vector2.zero;
        viewport.anchorMax = Vector2.one;
        viewport.offsetMin = Vector2.zero;
        viewport.offsetMax = Vector2.zero;
        Mask mask = viewport.gameObject.AddComponent<Mask>();
        mask.showMaskGraphic = false;

        GameObject listObject = new GameObject("Owned Item Rows");
        listObject.transform.SetParent(viewport, false);
        listRoot = listObject.AddComponent<RectTransform>();
        listRoot.anchorMin = new Vector2(0f, 1f);
        listRoot.anchorMax = new Vector2(1f, 1f);
        listRoot.pivot = new Vector2(0.5f, 1f);
        listRoot.offsetMin = Vector2.zero;
        listRoot.offsetMax = Vector2.zero;

        VerticalLayoutGroup layout = listObject.AddComponent<VerticalLayoutGroup>();
        layout.spacing = 2f;
        layout.childControlHeight = true;
        layout.childControlWidth = true;
        layout.childForceExpandHeight = false;
        layout.childForceExpandWidth = true;
        listObject.AddComponent<ContentSizeFitter>().verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        scrollRect.viewport = viewport;
        scrollRect.content = listRoot;

        controls.Initialize(panelRect, contentRoot);
        controls.CreateButton("-", new Vector2(-76f, -10f), controls.ToggleMinimize);
        controls.CreateButton("\u25A1", new Vector2(-42f, -10f), controls.ToggleMaximize);
        controls.CreateButton("x", new Vector2(-8f, -10f), controls.Close);
        controls.CreateResizeHandle(new Vector2(300f, 220f));
    }

    private void CreateHeader(Transform parent)
    {
        GameObject header = new GameObject("Owned Items Header");
        header.transform.SetParent(parent, false);
        RectTransform rect = header.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(1f, 1f);
        rect.offsetMin = new Vector2(0f, -34f);
        rect.offsetMax = Vector2.zero;

        CreateCell(header.transform, "\uC18C\uC720\uC790", 0f, 0.25f, 17);
        CreateCell(header.transform, "\uC774\uB984", 0.25f, 0.55f, 17);
        CreateCell(header.transform, "\uC885\uB958", 0.55f, 0.78f, 17);
        CreateCell(header.transform, "\uAC1C\uC218", 0.78f, 1f, 17);
    }

    private void EnsureRowCount(int count)
    {
        while (rows.Count < count)
        {
            rows.Add(CreateRow(listRoot));
        }
    }

    private Text[] CreateRow(Transform parent)
    {
        GameObject row = new GameObject("Owned Item Row");
        row.transform.SetParent(parent, false);

        RectTransform rect = row.AddComponent<RectTransform>();
        rect.sizeDelta = new Vector2(0f, 36f);

        LayoutElement layout = row.AddComponent<LayoutElement>();
        layout.preferredHeight = 36f;
        layout.minHeight = 36f;

        Image image = row.AddComponent<Image>();
        image.color = new Color(0.12f, 0.12f, 0.12f, 0.94f);

        return new[]
        {
            CreateCell(row.transform, string.Empty, 0f, 0.25f, 17),
            CreateCell(row.transform, string.Empty, 0.25f, 0.55f, 17),
            CreateCell(row.transform, string.Empty, 0.55f, 0.78f, 17),
            CreateCell(row.transform, string.Empty, 0.78f, 1f, 17)
        };
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

    private RectTransform CreatePanel(string name, Transform parent, Color color)
    {
        GameObject panel = new GameObject(name, typeof(RectTransform), typeof(Image));
        panel.transform.SetParent(parent, false);
        panel.GetComponent<Image>().color = color;
        return panel.GetComponent<RectTransform>();
    }

    private Text CreateCell(Transform parent, string value, float xMin, float xMax, int size)
    {
        Text text = CreateText(parent, value, size, TextAnchor.MiddleLeft);
        RectTransform rect = text.rectTransform;
        rect.anchorMin = new Vector2(xMin, 0f);
        rect.anchorMax = new Vector2(xMax, 1f);
        rect.offsetMin = new Vector2(8f, 0f);
        rect.offsetMax = new Vector2(-8f, 0f);
        return text;
    }

    private static string GetDisplayName(string itemId)
    {
        if (itemId.StartsWith("leaf_"))
        {
            return itemId.Replace("leaf_", "\uB098\uBB47\uC78E ");
        }

        if (itemId.StartsWith("branch_"))
        {
            return itemId.Replace("branch_", "\uB098\uBB34\uAC00\uC9C0 ");
        }

        if (itemId.StartsWith("wood_"))
        {
            return itemId.Replace("wood_", "\uB098\uBB34 ");
        }

        if (itemId.StartsWith("sand_"))
        {
            return itemId.Replace("sand_", "\uBAA8\uB798 ");
        }

        if (itemId.StartsWith("stone_"))
        {
            return itemId.Replace("stone_", "\uB3CC ");
        }

        if (itemId.StartsWith("dirt_"))
        {
            return itemId.Replace("dirt_", "\uD759 ");
        }

        if (itemId.StartsWith("coal_"))
        {
            return itemId.Replace("coal_", "\uC11D\uD0C4 ");
        }

        if (itemId.StartsWith("copper_"))
        {
            return itemId.Replace("copper_", "\uAD6C\uB9AC ");
        }

        if (itemId == "lead")
        {
            return "\uB0A9";
        }

        if (itemId.StartsWith("tin_"))
        {
            return itemId.Replace("tin_", "\uC8FC\uC11D ");
        }

        if (itemId.StartsWith("iron_"))
        {
            return itemId.Replace("iron_", "\uCCA0 ");
        }

        if (itemId.StartsWith("water_"))
        {
            return itemId.Replace("water_", "\uBB3C ");
        }

        if (itemId.StartsWith("flint_"))
        {
            return itemId.Replace("flint_", "\uBD80\uC2EF\uB3CC ");
        }

        if (itemId == "branch")
        {
            return "Branch";
        }

        if (itemId == "stone")
        {
            return "Stone";
        }

        return itemId;
    }

    private static string GetCategory(string itemId)
    {
        return itemId.StartsWith("leaf_")
            || itemId.StartsWith("branch_")
            || itemId.StartsWith("wood_")
            || itemId.StartsWith("sand_")
            || itemId.StartsWith("stone_")
            || itemId.StartsWith("dirt_")
            || itemId.StartsWith("coal_")
            || itemId.StartsWith("copper_")
            || itemId == "lead"
            || itemId.StartsWith("tin_")
            || itemId.StartsWith("iron_")
            || itemId.StartsWith("water_")
            || itemId.StartsWith("flint_")
            || itemId == "branch" || itemId == "stone" || itemId == "wood1" || itemId == "stone1"
            ? "\uC7AC\uB8CC"
            : "-";
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

    private readonly struct OwnedItemRowData
    {
        public readonly string OwnerName;
        public readonly string ItemId;
        public readonly int Count;

        public OwnedItemRowData(string ownerName, string itemId, int count)
        {
            OwnerName = ownerName;
            ItemId = itemId;
            Count = count;
        }
    }
}
