using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.Events;
using UnityEngine.UI;

namespace Dbd.Crafting
{
    public sealed class BlueprintWindowController : MonoBehaviour
    {
        public static BlueprintWindowController Instance { get; private set; }

        [SerializeField] private BlueprintDatabase database;
        [SerializeField] private int maxVisibleResults = 200;

        private readonly List<BlueprintItem> searchResults = new List<BlueprintItem>(256);
        private readonly List<Button> resultButtons = new List<Button>(256);
        private InputField searchInput;
        private Text detailText;
        private Text resultCountText;
        private Transform resultRoot;
        private RectTransform windowRoot;
        private Font font;

        private void Awake()
        {
            Instance = this;
            font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
            if (database == null)
            {
                database = BlueprintSampleData.CreateDatabase();
            }

            database.EnsureIndex();
            BuildUi();
            RefreshResults(string.Empty);
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
                if (windowRoot != null)
                {
                    windowRoot.gameObject.SetActive(true);
                }

                RefreshResults(searchInput != null ? searchInput.text : string.Empty);
            }
        }

        public void Show()
        {
            gameObject.SetActive(true);
            if (windowRoot != null)
            {
                windowRoot.gameObject.SetActive(true);
            }

            RefreshResults(searchInput != null ? searchInput.text : string.Empty);
        }

        private void BuildUi()
        {
            Canvas canvas = gameObject.GetComponent<Canvas>();
            if (canvas == null)
            {
                canvas = gameObject.AddComponent<Canvas>();
            }

            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.sortingOrder = 1220;
            gameObject.AddComponent<CanvasScaler>().uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            gameObject.AddComponent<GraphicRaycaster>();
            EnsureEventSystem();

            RectTransform root = CreatePanel("Blueprint Window", transform, new Color(0.08f, 0.09f, 0.1f, 0.94f));
            windowRoot = root;
            root.anchorMin = new Vector2(0f, 1f);
            root.anchorMax = new Vector2(0f, 1f);
            root.pivot = new Vector2(0f, 1f);
            root.anchoredPosition = new Vector2(90f, -90f);
            root.sizeDelta = new Vector2(700f, 460f);

            RectTransform titleBar = CreatePanel("Title Bar", root, new Color(0.10f, 0.10f, 0.10f, 0.98f));
            SetRect(titleBar, new Vector2(0f, 1f), new Vector2(1f, 1f), Vector2.zero, new Vector2(0f, -58f));

            RectTransform contentRoot = CreatePanel("Window Content", root, new Color(0f, 0f, 0f, 0f));
            contentRoot.GetComponent<Image>().raycastTarget = false;
            SetRect(contentRoot, Vector2.zero, Vector2.one, Vector2.zero, new Vector2(0f, -58f));

            Text title = CreateText("청사진", titleBar, 24, FontStyle.Bold, TextAnchor.MiddleLeft);
            SetRect(title.rectTransform, Vector2.zero, Vector2.one, new Vector2(18f, 0f), new Vector2(-120f, 0f));

            searchInput = CreateInput(contentRoot);
            SetRect(searchInput.GetComponent<RectTransform>(), new Vector2(0f, 1f), new Vector2(1f, 1f), new Vector2(18f, -54f), new Vector2(-18f, -10f));
            searchInput.onValueChanged.AddListener(RefreshResults);

            resultCountText = CreateText("검색 결과: 0", contentRoot, 16, FontStyle.Normal, TextAnchor.MiddleLeft);
            SetRect(resultCountText.rectTransform, new Vector2(0f, 1f), new Vector2(0.42f, 1f), new Vector2(18f, -82f), new Vector2(-10f, -56f));

            RectTransform listPanel = CreatePanel("Result List", contentRoot, new Color(0.13f, 0.14f, 0.15f, 0.95f));
            SetRect(listPanel, new Vector2(0f, 0f), new Vector2(0.42f, 1f), new Vector2(18f, 18f), new Vector2(-10f, -92f));

            ScrollRect listScroll = listPanel.gameObject.AddComponent<ScrollRect>();
            listScroll.horizontal = false;
            listScroll.scrollSensitivity = 85f;

            RectTransform listViewport = CreatePanel("Viewport", listPanel, new Color(0f, 0f, 0f, 0f));
            SetRect(listViewport, Vector2.zero, Vector2.one, Vector2.zero, Vector2.zero);
            Mask mask = listViewport.gameObject.AddComponent<Mask>();
            mask.showMaskGraphic = false;
            listViewport.GetComponent<Image>().color = new Color(0f, 0f, 0f, 0.01f);

            RectTransform content = CreatePanel("Content", listViewport, new Color(0f, 0f, 0f, 0f));
            content.anchorMin = new Vector2(0f, 1f);
            content.anchorMax = new Vector2(1f, 1f);
            content.pivot = new Vector2(0.5f, 1f);
            content.offsetMin = new Vector2(8f, 0f);
            content.offsetMax = new Vector2(-8f, 0f);
            VerticalLayoutGroup layout = content.gameObject.AddComponent<VerticalLayoutGroup>();
            layout.spacing = 6f;
            layout.padding = new RectOffset(0, 0, 8, 8);
            layout.childControlHeight = true;
            layout.childForceExpandHeight = false;
            content.gameObject.AddComponent<ContentSizeFitter>().verticalFit = ContentSizeFitter.FitMode.PreferredSize;

            listScroll.viewport = listViewport;
            listScroll.content = content;
            resultRoot = content;

            RectTransform detailPanel = CreatePanel("Detail Panel", contentRoot, new Color(0.12f, 0.13f, 0.14f, 0.95f));
            SetRect(detailPanel, new Vector2(0.42f, 0f), Vector2.one, new Vector2(10f, 18f), new Vector2(-18f, -68f));

            ScrollRect detailScroll = detailPanel.gameObject.AddComponent<ScrollRect>();
            detailScroll.horizontal = false;
            detailScroll.scrollSensitivity = 85f;

            RectTransform detailViewport = CreatePanel("Detail Viewport", detailPanel, new Color(0f, 0f, 0f, 0f));
            SetRect(detailViewport, Vector2.zero, Vector2.one, Vector2.zero, Vector2.zero);
            Mask detailMask = detailViewport.gameObject.AddComponent<Mask>();
            detailMask.showMaskGraphic = false;
            detailViewport.GetComponent<Image>().color = new Color(0f, 0f, 0f, 0.01f);

            detailText = CreateText("아이템을 선택하세요.", detailViewport, 18, FontStyle.Normal, TextAnchor.UpperLeft);
            detailText.horizontalOverflow = HorizontalWrapMode.Wrap;
            detailText.verticalOverflow = VerticalWrapMode.Overflow;
            detailText.rectTransform.anchorMin = new Vector2(0f, 1f);
            detailText.rectTransform.anchorMax = new Vector2(1f, 1f);
            detailText.rectTransform.pivot = new Vector2(0.5f, 1f);
            detailText.rectTransform.offsetMin = new Vector2(14f, 0f);
            detailText.rectTransform.offsetMax = new Vector2(-14f, -14f);
            detailText.gameObject.AddComponent<ContentSizeFitter>().verticalFit = ContentSizeFitter.FitMode.PreferredSize;

            detailScroll.viewport = detailViewport;
            detailScroll.content = detailText.rectTransform;

            RuntimeWindowControls controls = root.gameObject.AddComponent<RuntimeWindowControls>();
            controls.Initialize(root, contentRoot);
            controls.CreateButton("-", new Vector2(-76f, -10f), controls.ToggleMinimize);
            controls.CreateButton("□", new Vector2(-42f, -10f), controls.ToggleMaximize);
            controls.CreateButton("x", new Vector2(-8f, -10f), controls.Close);
            controls.CreateResizeHandle(new Vector2(480f, 320f));
        }

        private void RefreshResults(string query)
        {
            database.SearchItems(query, searchResults);
            if (resultCountText != null)
            {
                resultCountText.text = "검색 결과: " + searchResults.Count;
            }

            while (resultButtons.Count < searchResults.Count && resultButtons.Count < maxVisibleResults)
            {
                resultButtons.Add(CreateResultButton(resultRoot));
            }

            for (int index = 0; index < resultButtons.Count; index++)
            {
                bool visible = index < searchResults.Count && index < maxVisibleResults;
                resultButtons[index].gameObject.SetActive(visible);

                if (!visible)
                {
                    continue;
                }

                BlueprintItem item = searchResults[index];
                resultButtons[index].GetComponentInChildren<Text>().text = item.DisplayName + "  [" + item.Category + "]";
                resultButtons[index].onClick.RemoveAllListeners();
                resultButtons[index].onClick.AddListener(new UnityAction(() => ShowDetails(item)));
            }

            if (searchResults.Count == 0)
            {
                detailText.text = "검색 결과가 없습니다.";
            }
            else if (searchResults.Count > maxVisibleResults)
            {
                detailText.text = "검색 결과 " + searchResults.Count + "개 중 " + maxVisibleResults + "개만 표시 중입니다.\n검색어를 더 자세히 입력하세요.";
            }
            else if (!string.IsNullOrWhiteSpace(query))
            {
                detailText.text = "검색 결과 " + searchResults.Count + "개가 표시 중입니다.\n왼쪽 목록에서 아이템을 누르면 제작법이 나옵니다.";
            }
        }

        private void ShowDetails(BlueprintItem item)
        {
            StringBuilder builder = new StringBuilder(1024);
            builder.AppendLine(item.DisplayName);
            builder.AppendLine("분류: " + item.Category);
            builder.AppendLine();

            AppendRecipes(builder, "이 아이템을 만드는 법", database.GetRecipesForResult(item.Id));
            builder.AppendLine();
            AppendRecipes(builder, "이 아이템으로 만들 수 있는 것", database.GetRecipesUsingIngredient(item.Id));

            detailText.text = builder.ToString();
        }

        private void AppendRecipes(StringBuilder builder, string title, IReadOnlyList<BlueprintRecipe> recipes)
        {
            builder.AppendLine(title);
            if (recipes.Count == 0)
            {
                builder.AppendLine("- 없음");
                return;
            }

            foreach (BlueprintRecipe recipe in recipes)
            {
                BlueprintItem resultItem = database.GetItem(recipe.ResultItemId);
                builder.Append("- ");
                builder.Append(resultItem != null ? resultItem.DisplayName : recipe.ResultItemId);
                builder.Append(" x");
                builder.Append(recipe.ResultAmount);
                builder.Append(": ");

                for (int index = 0; index < recipe.Ingredients.Count; index++)
                {
                    BlueprintIngredient ingredient = recipe.Ingredients[index];
                    BlueprintItem ingredientItem = database.GetItem(ingredient.ItemId);
                    builder.Append(ingredientItem != null ? ingredientItem.DisplayName : ingredient.ItemId);
                    builder.Append(" x");
                    builder.Append(ingredient.Amount);

                    if (index < recipe.Ingredients.Count - 1)
                    {
                        builder.Append(" + ");
                    }
                }

                builder.AppendLine();
            }
        }

        private Button CreateResultButton(Transform parent)
        {
            Button button = CreateButton("Result Button", parent, "아이템");
            RectTransform rect = button.GetComponent<RectTransform>();
            rect.sizeDelta = new Vector2(0f, 36f);
            LayoutElement layout = button.gameObject.AddComponent<LayoutElement>();
            layout.minHeight = 36f;
            layout.preferredHeight = 36f;
            return button;
        }

        private InputField CreateInput(Transform parent)
        {
            RectTransform inputRoot = CreatePanel("Search Input", parent, new Color(0.95f, 0.95f, 0.95f, 1f));
            InputField input = inputRoot.gameObject.AddComponent<InputField>();

            Text text = CreateText(string.Empty, inputRoot, 18, FontStyle.Normal, TextAnchor.MiddleLeft);
            text.color = Color.black;
            SetRect(text.rectTransform, Vector2.zero, Vector2.one, new Vector2(12f, 0f), new Vector2(-12f, 0f));

            Text placeholder = CreateText("이름 검색: 예) 벽", inputRoot, 18, FontStyle.Italic, TextAnchor.MiddleLeft);
            placeholder.color = new Color(0.35f, 0.35f, 0.35f, 0.8f);
            SetRect(placeholder.rectTransform, Vector2.zero, Vector2.one, new Vector2(12f, 0f), new Vector2(-12f, 0f));

            input.textComponent = text;
            input.placeholder = placeholder;
            return input;
        }

        private Button CreateButton(string name, Transform parent, string label)
        {
            RectTransform root = CreatePanel(name, parent, new Color(0.22f, 0.24f, 0.26f, 1f));
            Button button = root.gameObject.AddComponent<Button>();
            ColorBlock colors = button.colors;
            colors.highlightedColor = new Color(0.33f, 0.38f, 0.42f, 1f);
            colors.pressedColor = new Color(0.16f, 0.18f, 0.2f, 1f);
            button.colors = colors;

            Text text = CreateText(label, root, 16, FontStyle.Normal, TextAnchor.MiddleLeft);
            SetRect(text.rectTransform, Vector2.zero, Vector2.one, new Vector2(12f, 0f), new Vector2(-12f, 0f));
            return button;
        }

        private RectTransform CreatePanel(string name, Transform parent, Color color)
        {
            GameObject panel = new GameObject(name, typeof(RectTransform), typeof(Image));
            panel.transform.SetParent(parent, false);
            panel.GetComponent<Image>().color = color;
            return panel.GetComponent<RectTransform>();
        }

        private Text CreateText(string text, Transform parent, int size, FontStyle style, TextAnchor alignment)
        {
            GameObject textObject = new GameObject("Text", typeof(RectTransform), typeof(Text));
            textObject.transform.SetParent(parent, false);
            Text label = textObject.GetComponent<Text>();
            label.font = font;
            label.text = text;
            label.fontSize = size;
            label.fontStyle = style;
            label.alignment = alignment;
            label.color = Color.white;
            return label;
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
}
