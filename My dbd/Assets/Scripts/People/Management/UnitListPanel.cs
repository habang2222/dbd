using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class UnitListPanel : MonoBehaviour
{
    public static UnitListPanel Instance { get; private set; }

    private readonly List<Button> unitButtons = new();
    private readonly List<PersonComponent> displayedPeople = new();
    private RectTransform listRoot;
    private RectTransform contentRoot;
    private GameObject canvasObject;
    private GameObject windowObject;
    private Text titleText;
    private Font font;
    private PersonManager personManager;
    private float nextRefreshTime;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<UnitListPanel>() != null)
        {
            return;
        }

        GameObject panelObject = new GameObject("Unit List Panel");
        panelObject.AddComponent<UnitListPanel>();
    }

    private void Awake()
    {
        Instance = this;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        RefreshList();
        SetWindowVisible(false);
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    private void Update()
    {
        if (Time.unscaledTime < nextRefreshTime)
        {
            return;
        }

        nextRefreshTime = Time.unscaledTime + 0.1f;
        RefreshList();
    }

    public void RefreshList()
    {
        PersonComponent[] people = FindPeople();
        titleText.text = $"\uC720\uB2DB \uBAA9\uB85D ({people.Length})";

        EnsureButtonCount(people.Length);

        displayedPeople.Clear();
        for (int i = 0; i < people.Length; i++)
        {
            PersonComponent person = people[i];
            displayedPeople.Add(person);

            Button button = unitButtons[i];
            button.gameObject.SetActive(true);
            button.image.color = GetRowColor(person);

            Text[] cells = button.GetComponentsInChildren<Text>();
            cells[0].text = GetUnitIcon(i);
            cells[1].text = person.PersonName;
            PersonStats stats = person.Stats;
            cells[2].text = $"{stats.health:0}";
            cells[3].text = $"{stats.strength:0}";
            cells[4].text = $"{stats.stamina:0}";
            cells[5].text = GetDisplayState(person);
            cells[6].text = GetDisplayAction(person);
            cells[7].text = string.IsNullOrWhiteSpace(person.TeamId) ? "-" : person.TeamId;
            cells[8].text = person.IsSelected ? "Y" : "N";

            int index = i;
            button.onClick.RemoveAllListeners();
            button.onClick.AddListener(() => SelectDisplayedPerson(index));
        }

        for (int i = people.Length; i < unitButtons.Count; i++)
        {
            unitButtons[i].gameObject.SetActive(false);
        }
    }

    public void Toggle()
    {
        bool shouldShow = windowObject == null || !windowObject.activeSelf;
        SetWindowVisible(shouldShow);
        if (shouldShow)
        {
            RefreshList();
        }
    }

    public void Show()
    {
        SetWindowVisible(true);
        RefreshList();
    }

    private void SetWindowVisible(bool isVisible)
    {
        if (canvasObject != null)
        {
            canvasObject.SetActive(isVisible);
        }

        if (windowObject != null)
        {
            windowObject.SetActive(isVisible);
        }

        if (isVisible)
        {
            gameObject.SetActive(true);
        }
    }

    private PersonComponent[] FindPeople()
    {
        if (personManager == null)
        {
            personManager = FindFirstObjectByType<PersonManager>();
        }

        if (personManager != null && personManager.Persons.Count > 0)
        {
            PersonComponent[] managedPeople = new PersonComponent[personManager.Persons.Count];
            for (int i = 0; i < managedPeople.Length; i++)
            {
                managedPeople[i] = personManager.Persons[i];
            }

            return managedPeople;
        }

        PersonComponent[] scenePeople = FindObjectsByType<PersonComponent>(FindObjectsSortMode.None);
        System.Array.Sort(scenePeople, (left, right) => string.Compare(left.PersonName, right.PersonName, System.StringComparison.Ordinal));
        return scenePeople;
    }

    private void SelectDisplayedPerson(int index)
    {
        if (index < 0 || index >= displayedPeople.Count)
        {
            return;
        }

        PersonComponent person = displayedPeople[index];
        if (person == null)
        {
            return;
        }

        person.Select();
        FocusCameraOn(person.transform.position);
        RefreshList();
    }

    private void CreateUi()
    {
        canvasObject = new GameObject("Unit List Canvas");
        canvasObject.transform.SetParent(transform, false);

        Canvas canvas = canvasObject.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 999;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Unit List Window");
        panel.transform.SetParent(canvas.transform, false);
        windowObject = panel;

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(0f, 1f);
        panelRect.anchorMax = new Vector2(0f, 1f);
        panelRect.pivot = new Vector2(0f, 1f);
        panelRect.anchoredPosition = new Vector2(24f, -82f);
        panelRect.sizeDelta = new Vector2(980f, 360f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.09f, 0.09f, 0.09f, 0.94f);

        RuntimeWindowControls controls = panel.AddComponent<RuntimeWindowControls>();

        titleText = CreateText(panel.transform, "\uC720\uB2DB \uBAA9\uB85D", 24, TextAnchor.MiddleLeft);
        RectTransform titleRect = titleText.GetComponent<RectTransform>();
        titleRect.anchorMin = new Vector2(0f, 1f);
        titleRect.anchorMax = new Vector2(1f, 1f);
        titleRect.pivot = new Vector2(0.5f, 1f);
        titleRect.offsetMin = new Vector2(24f, -56f);
        titleRect.offsetMax = new Vector2(-24f, -12f);

        GameObject contentObject = new GameObject("Window Content");
        contentObject.transform.SetParent(panel.transform, false);
        contentRoot = contentObject.AddComponent<RectTransform>();
        contentRoot.anchorMin = Vector2.zero;
        contentRoot.anchorMax = Vector2.one;
        contentRoot.offsetMin = Vector2.zero;
        contentRoot.offsetMax = Vector2.zero;
        contentObject.AddComponent<RectMask2D>();

        Text subtitle = CreateText(contentRoot, "\uC720\uB2DB \uC0C1\uD0DC\uC640 \uD604\uC7AC \uBA85\uB839", 16, TextAnchor.MiddleLeft);
        RectTransform subtitleRect = subtitle.GetComponent<RectTransform>();
        subtitleRect.anchorMin = new Vector2(0f, 1f);
        subtitleRect.anchorMax = new Vector2(1f, 1f);
        subtitleRect.offsetMin = new Vector2(24f, -92f);
        subtitleRect.offsetMax = new Vector2(-24f, -58f);

        CreateHeader(contentRoot);

        RectTransform listPanel = CreatePanel("Unit Table Scroll", contentRoot, new Color(0f, 0f, 0f, 0.01f));
        listPanel.anchorMin = new Vector2(0f, 0f);
        listPanel.anchorMax = new Vector2(1f, 1f);
        listPanel.offsetMin = new Vector2(24f, 18f);
        listPanel.offsetMax = new Vector2(-24f, -136f);
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

        GameObject listObject = new GameObject("Unit Table Rows");
        listObject.transform.SetParent(viewport, false);
        listRoot = listObject.AddComponent<RectTransform>();
        listRoot.anchorMin = new Vector2(0f, 1f);
        listRoot.anchorMax = new Vector2(1f, 1f);
        listRoot.pivot = new Vector2(0.5f, 1f);
        listRoot.offsetMin = Vector2.zero;
        listRoot.offsetMax = Vector2.zero;

        VerticalLayoutGroup layout = listObject.AddComponent<VerticalLayoutGroup>();
        layout.spacing = 0f;
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
        controls.CreateResizeHandle(new Vector2(620f, 220f));
    }

    private void EnsureButtonCount(int count)
    {
        unitButtons.RemoveAll(button => button == null);

        while (unitButtons.Count < count)
        {
            unitButtons.Add(CreateUnitButton(listRoot));
        }
    }

    private Button CreateUnitButton(Transform parent)
    {
        GameObject buttonObject = new GameObject("Unit Table Row");
        buttonObject.transform.SetParent(parent, false);

        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.sizeDelta = new Vector2(0f, 48f);

        LayoutElement layout = buttonObject.AddComponent<LayoutElement>();
        layout.preferredHeight = 48f;
        layout.minHeight = 48f;

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.10f, 0.10f, 0.10f, 0.92f);

        Button button = buttonObject.AddComponent<Button>();

        CreateCell(buttonObject.transform, string.Empty, 0f, 0.07f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.07f, 0.22f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.22f, 0.31f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.31f, 0.40f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.40f, 0.49f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.49f, 0.63f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.63f, 0.82f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.82f, 0.92f, 15);
        CreateCell(buttonObject.transform, string.Empty, 0.92f, 1f, 15);

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

    private void CreateHeader(Transform parent)
    {
        GameObject header = new GameObject("Unit Table Header");
        header.transform.SetParent(parent, false);

        RectTransform rect = header.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(1f, 1f);
        rect.offsetMin = new Vector2(24f, -134f);
        rect.offsetMax = new Vector2(-24f, -98f);

        CreateCell(header.transform, "\uC544\uC774\uCF58", 0f, 0.07f, 15);
        CreateCell(header.transform, "\uC774\uB984", 0.07f, 0.22f, 15);
        CreateCell(header.transform, "HP", 0.22f, 0.31f, 15);
        CreateCell(header.transform, "\uD798", 0.31f, 0.40f, 15);
        CreateCell(header.transform, "\uC2A4\uD14C\uBBF8\uB098", 0.40f, 0.49f, 15);
        CreateCell(header.transform, "\uC0C1\uD0DC", 0.49f, 0.63f, 15);
        CreateCell(header.transform, "\uD589\uB3D9", 0.63f, 0.82f, 15);
        CreateCell(header.transform, "\uD300", 0.82f, 0.92f, 15);
        CreateCell(header.transform, "\uC120\uD0DD", 0.92f, 1f, 15);
    }

    private Text CreateCell(Transform parent, string value, float xMin, float xMax, int size)
    {
        Text text = CreateText(parent, value, size, TextAnchor.MiddleLeft);
        RectTransform rect = text.GetComponent<RectTransform>();
        rect.anchorMin = new Vector2(xMin, 0f);
        rect.anchorMax = new Vector2(xMax, 1f);
        rect.offsetMin = new Vector2(8f, 0f);
        rect.offsetMax = new Vector2(-8f, 0f);
        return text;
    }

    private static void FocusCameraOn(Vector3 position)
    {
        CameraPanZoomController controller = FindFirstObjectByType<CameraPanZoomController>();
        if (controller != null)
        {
            controller.FocusOn(position);
            return;
        }

        if (Camera.main != null)
        {
            Vector3 cameraPosition = Camera.main.transform.position;
            cameraPosition.x = position.x;
            cameraPosition.z = position.z - 6f;
            Camera.main.transform.position = cameraPosition;
        }
    }

    private static string GetUnitIcon(int index)
    {
        string[] icons = { "R", "S", "C", "F", "P" };
        return icons[index % icons.Length];
    }

    private static string GetDisplayState(PersonComponent person)
    {
        if (UnitCombatController.IsRetreating(person) && IsNearAnyEnemy(person, 5f))
        {
            return "\uD6C4\uD1F4 \uC911";
        }

        if (UnitCombatController.IsPersonInCombat(person))
        {
            return "\uC804\uD22C \uC911";
        }

        PersonMover mover = person.GetComponent<PersonMover>();
        if (mover != null && mover.IsRunning)
        {
            return "\uB2EC\uB9AC\uB294 \uC911";
        }

        if (mover != null && mover.IsMoving)
        {
            return "\uC774\uB3D9 \uC911";
        }

        return "\uB300\uAE30";
    }

    private static string GetDisplayAction(PersonComponent person)
    {
        if (UnitCombatController.IsRetreating(person) && IsNearAnyEnemy(person, 5f))
        {
            return "\uC7AC\uC815\uBE44";
        }

        if (UnitCombatController.IsPersonInCombat(person))
        {
            return "\uD6C4\uD1F4 \uD544\uC694";
        }

        if (!string.IsNullOrWhiteSpace(person.CurrentAction) && person.CurrentAction != "None")
        {
            return person.CurrentAction;
        }

        return "-";
    }

    private static Color GetRowColor(PersonComponent person)
    {
        if (UnitCombatController.IsPersonInCombat(person))
        {
            float blink = Mathf.PingPong(Time.unscaledTime * 4f, 1f);
            return Color.Lerp(new Color(0.18f, 0.04f, 0.04f, 0.95f), new Color(0.78f, 0.05f, 0.04f, 0.95f), blink);
        }

        return person.IsSelected
            ? new Color(0.16f, 0.22f, 0.24f, 0.95f)
            : new Color(0.10f, 0.10f, 0.10f, 0.92f);
    }

    private static bool IsNearAnyEnemy(PersonComponent person, float range)
    {
        if (person == null || person.Stats.health <= 0f)
        {
            return false;
        }

        foreach (EnemyComponent enemy in FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None))
        {
            if (enemy == null || enemy.Stats.health <= 0f)
            {
                continue;
            }

            if (Vector3.Distance(person.transform.position, enemy.transform.position) <= range)
            {
                return true;
            }
        }

        return false;
    }

    private RectTransform CreatePanel(string name, Transform parent, Color color)
    {
        GameObject panel = new GameObject(name, typeof(RectTransform), typeof(Image));
        panel.transform.SetParent(parent, false);
        panel.GetComponent<Image>().color = color;
        return panel.GetComponent<RectTransform>();
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
