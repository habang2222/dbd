using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class WorkbenchCraftingStation : MonoBehaviour
{
    private readonly List<Dbd.Crafting.BlueprintItem> searchResults = new();
    private readonly List<Button> resultButtons = new();
    private const float CraftingRange = 4f;
    private Dbd.Crafting.BlueprintDatabase database;
    private Canvas canvas;
    private InputField searchInput;
    private RectTransform resultRoot;
    private Text detailText;
    private Text resultCountText;
    private Font font;

    private void Awake()
    {
        database = Dbd.Crafting.BlueprintSampleData.CreateDatabase();
        database.EnsureIndex();
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
    }

    private void OnMouseDown()
    {
        if (EventSystem.current != null && EventSystem.current.IsPointerOverGameObject())
        {
            return;
        }

        ToggleWindow();
    }

    private void ToggleWindow()
    {
        if (canvas == null)
        {
            CreateUi();
        }

        canvas.gameObject.SetActive(!canvas.gameObject.activeSelf);
        if (canvas.gameObject.activeSelf)
        {
            RefreshResults(searchInput != null ? searchInput.text : string.Empty);
        }
    }

    private void CreateUi()
    {
        canvas = new GameObject("Workbench Crafting Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1230;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Workbench Crafting Panel");
        panel.transform.SetParent(canvas.transform, false);
        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(0f, 1f);
        panelRect.anchorMax = new Vector2(0f, 1f);
        panelRect.pivot = new Vector2(0f, 1f);
        panelRect.anchoredPosition = new Vector2(120f, -110f);
        panelRect.sizeDelta = new Vector2(760f, 520f);
        panel.AddComponent<Image>().color = new Color(0.08f, 0.08f, 0.08f, 0.96f);

        RectTransform titleBar = CreatePanel("Title Bar", panel.transform, new Color(0.10f, 0.10f, 0.10f, 0.98f));
        SetRect(titleBar, new Vector2(0f, 1f), new Vector2(1f, 1f), Vector2.zero, new Vector2(0f, -58f));

        Text title = CreateText(titleBar, "제작대", 24, TextAnchor.MiddleLeft);
        SetRect(title.rectTransform, Vector2.zero, Vector2.one, new Vector2(18f, 0f), new Vector2(-112f, 0f));

        RectTransform contentRoot = CreatePanel("Window Content", panel.transform, new Color(0f, 0f, 0f, 0f));
        contentRoot.GetComponent<Image>().raycastTarget = false;
        SetRect(contentRoot, Vector2.zero, Vector2.one, Vector2.zero, new Vector2(0f, -58f));

        searchInput = CreateInput(contentRoot);
        SetRect(searchInput.GetComponent<RectTransform>(), new Vector2(0f, 1f), new Vector2(1f, 1f), new Vector2(18f, -54f), new Vector2(-18f, -10f));
        searchInput.onValueChanged.AddListener(RefreshResults);

        resultCountText = CreateText(contentRoot, "제작 가능: 0", 16, TextAnchor.MiddleLeft);
        SetRect(resultCountText.rectTransform, new Vector2(0f, 1f), new Vector2(0.45f, 1f), new Vector2(18f, -82f), new Vector2(-10f, -56f));

        RectTransform listPanel = CreatePanel("Recipe List Panel", contentRoot, new Color(0.13f, 0.14f, 0.15f, 0.95f));
        SetRect(listPanel, new Vector2(0f, 0f), new Vector2(0.45f, 1f), new Vector2(18f, 18f), new Vector2(-10f, -92f));

        ScrollRect listScroll = listPanel.gameObject.AddComponent<ScrollRect>();
        listScroll.horizontal = false;
        listScroll.scrollSensitivity = 85f;
        RectTransform viewport = CreatePanel("Viewport", listPanel, new Color(0f, 0f, 0f, 0.01f));
        SetRect(viewport, Vector2.zero, Vector2.one, Vector2.zero, Vector2.zero);
        Mask mask = viewport.gameObject.AddComponent<Mask>();
        mask.showMaskGraphic = false;

        RectTransform list = CreatePanel("Recipes", viewport, new Color(0f, 0f, 0f, 0f));
        list.anchorMin = new Vector2(0f, 1f);
        list.anchorMax = new Vector2(1f, 1f);
        list.pivot = new Vector2(0.5f, 1f);
        list.offsetMin = new Vector2(8f, 0f);
        list.offsetMax = new Vector2(-8f, 0f);
        VerticalLayoutGroup layout = list.gameObject.AddComponent<VerticalLayoutGroup>();
        layout.spacing = 5f;
        layout.padding = new RectOffset(0, 0, 8, 8);
        layout.childControlHeight = true;
        layout.childForceExpandHeight = false;
        list.gameObject.AddComponent<ContentSizeFitter>().verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        listScroll.viewport = viewport;
        listScroll.content = list;
        resultRoot = list;

        RectTransform detailPanel = CreatePanel("Detail Panel", contentRoot, new Color(0.12f, 0.13f, 0.14f, 0.95f));
        SetRect(detailPanel, new Vector2(0.45f, 0f), Vector2.one, new Vector2(10f, 18f), new Vector2(-18f, -68f));
        detailText = CreateText(detailPanel, "만들 물건을 누르면 제작합니다.", 18, TextAnchor.UpperLeft);
        detailText.horizontalOverflow = HorizontalWrapMode.Wrap;
        detailText.verticalOverflow = VerticalWrapMode.Overflow;
        SetRect(detailText.rectTransform, Vector2.zero, Vector2.one, new Vector2(14f, 14f), new Vector2(-14f, -14f));

        RuntimeWindowControls controls = titleBar.gameObject.AddComponent<RuntimeWindowControls>();
        controls.Initialize(panelRect, contentRoot);
        controls.CreateButton("-", new Vector2(-76f, -10f), controls.ToggleMinimize);
        controls.CreateButton("□", new Vector2(-42f, -10f), controls.ToggleMaximize);
        controls.CreateButton("x", new Vector2(-8f, -10f), () => canvas.gameObject.SetActive(false));
        controls.CreateResizeHandle(new Vector2(520f, 340f));

        canvas.gameObject.SetActive(false);
    }

    private void RefreshResults(string query)
    {
        database.SearchItems(query, searchResults);
        searchResults.RemoveAll(item => database.GetRecipesForResult(item.Id).Count == 0);
        resultCountText.text = "제작 가능 항목: " + searchResults.Count;

        while (resultButtons.Count < searchResults.Count)
        {
            resultButtons.Add(CreateRecipeButton(resultRoot));
        }

        PersonComponent person = FindSelectedPerson();
        for (int i = 0; i < resultButtons.Count; i++)
        {
            bool visible = i < searchResults.Count;
            resultButtons[i].gameObject.SetActive(visible);
            if (!visible)
            {
                continue;
            }

            Dbd.Crafting.BlueprintItem item = searchResults[i];
            Dbd.Crafting.BlueprintRecipe recipe = database.GetRecipesForResult(item.Id)[0];
            bool canCraft = CraftingService.CanCraft(person, recipe);
            resultButtons[i].GetComponentInChildren<Text>().text = (canCraft ? "제작 " : "부족 ") + item.DisplayName;
            resultButtons[i].image.color = canCraft
                ? new Color(0.18f, 0.28f, 0.18f, 0.98f)
                : new Color(0.24f, 0.14f, 0.14f, 0.98f);
            resultButtons[i].onClick.RemoveAllListeners();
            resultButtons[i].onClick.AddListener(() => TryCraft(item, recipe));
        }
    }

    private void TryCraft(Dbd.Crafting.BlueprintItem item, Dbd.Crafting.BlueprintRecipe recipe)
    {
        PersonComponent person = FindSelectedPerson();
        if (person == null)
        {
            detailText.text = "제작할 유닛을 먼저 선택하세요.";
            return;
        }

        if (!IsPersonInCraftingRange(person))
        {
            detailText.text = "제작대 가까이 있는 유닛만 제작할 수 있습니다.";
            RefreshResults(searchInput.text);
            return;
        }

        if (!CraftingService.CanCraft(person, recipe))
        {
            detailText.text = BuildRecipeText(item, recipe, person, false);
            RefreshResults(searchInput.text);
            return;
        }

        CraftingService.TryCraft(person, item, recipe, out string craftMessage);
        detailText.text = craftMessage + "\n\n" + BuildRecipeText(item, recipe, person, true);
        RefreshResults(searchInput.text);
    }

    private bool IsPersonInCraftingRange(PersonComponent person)
    {
        return person != null && Vector3.Distance(person.transform.position, transform.position) <= CraftingRange;
    }

    private string BuildRecipeText(Dbd.Crafting.BlueprintItem item, Dbd.Crafting.BlueprintRecipe recipe, PersonComponent person, bool afterCraft)
    {
        StringBuilder builder = new();
        builder.AppendLine(item.DisplayName);
        builder.AppendLine(afterCraft ? "방금 제작했습니다." : "재료가 부족합니다.");
        builder.AppendLine();
        builder.AppendLine("필요 재료");
        foreach (Dbd.Crafting.BlueprintIngredient ingredient in recipe.Ingredients)
        {
            int owned = InventoryService.GetItemCount(person, ingredient.ItemId);
            builder.Append("- ");
            builder.Append(ingredient.ItemId);
            builder.Append(" ");
            builder.Append(owned);
            builder.Append("/");
            builder.AppendLine(ingredient.Amount.ToString());
        }

        return builder.ToString();
    }

    private Button CreateRecipeButton(Transform parent)
    {
        RectTransform root = CreatePanel("Recipe Button", parent, new Color(0.18f, 0.18f, 0.18f, 0.98f));
        Button button = root.gameObject.AddComponent<Button>();
        LayoutElement layout = root.gameObject.AddComponent<LayoutElement>();
        layout.minHeight = 40f;
        layout.preferredHeight = 40f;
        Text label = CreateText(root, string.Empty, 17, TextAnchor.MiddleLeft);
        SetRect(label.rectTransform, Vector2.zero, Vector2.one, new Vector2(10f, 0f), new Vector2(-10f, 0f));
        return button;
    }

    private InputField CreateInput(Transform parent)
    {
        RectTransform inputRoot = CreatePanel("Search Input", parent, new Color(0.95f, 0.95f, 0.95f, 1f));
        InputField input = inputRoot.gameObject.AddComponent<InputField>();
        Text text = CreateText(inputRoot, string.Empty, 18, TextAnchor.MiddleLeft);
        text.color = Color.black;
        SetRect(text.rectTransform, Vector2.zero, Vector2.one, new Vector2(10f, 0f), new Vector2(-10f, 0f));
        Text placeholder = CreateText(inputRoot, "제작 검색", 18, TextAnchor.MiddleLeft);
        placeholder.color = new Color(0.35f, 0.35f, 0.35f, 0.8f);
        SetRect(placeholder.rectTransform, Vector2.zero, Vector2.one, new Vector2(10f, 0f), new Vector2(-10f, 0f));
        input.textComponent = text;
        input.placeholder = placeholder;
        return input;
    }

    private RectTransform CreatePanel(string name, Transform parent, Color color)
    {
        GameObject panel = new GameObject(name, typeof(RectTransform), typeof(Image));
        panel.transform.SetParent(parent, false);
        panel.GetComponent<Image>().color = color;
        return panel.GetComponent<RectTransform>();
    }

    private Text CreateText(Transform parent, string value, int size, TextAnchor alignment)
    {
        GameObject textObject = new GameObject("Text", typeof(RectTransform), typeof(Text));
        textObject.transform.SetParent(parent, false);
        Text text = textObject.GetComponent<Text>();
        text.font = font;
        text.fontSize = size;
        text.alignment = alignment;
        text.color = Color.white;
        text.text = value;
        return text;
    }

    private static PersonComponent FindSelectedPerson()
    {
        foreach (PersonComponent person in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (person.IsSelected)
            {
                return person;
            }
        }

        return null;
    }

    private static void SetRect(RectTransform rect, Vector2 anchorMin, Vector2 anchorMax, Vector2 offsetMin, Vector2 offsetMax)
    {
        rect.anchorMin = anchorMin;
        rect.anchorMax = anchorMax;
        rect.offsetMin = offsetMin;
        rect.offsetMax = offsetMax;
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
