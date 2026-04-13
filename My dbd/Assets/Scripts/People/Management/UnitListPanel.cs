using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class UnitListPanel : MonoBehaviour
{
    public static UnitListPanel Instance { get; private set; }

    private readonly List<Button> unitButtons = new();
    private readonly List<Image> healthBars = new();
    private readonly List<Image> shieldBars = new();
    private readonly List<PersonComponent> displayedPeople = new();
    private RectTransform listRoot;
    private RectTransform contentRoot;
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

        nextRefreshTime = Time.unscaledTime + 0.25f;
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
            button.image.color = person.IsSelected
                ? new Color(0.16f, 0.22f, 0.24f, 0.95f)
                : new Color(0.10f, 0.10f, 0.10f, 0.92f);

            Text[] cells = button.GetComponentsInChildren<Text>();
            cells[0].text = GetUnitIcon(i);
            cells[1].text = person.PersonName;
            cells[2].text = GetDisplayState(person);
            cells[3].text = GetDisplayAction(person);

            PersonStats stats = person.Stats;
            healthBars[i].fillAmount = Mathf.Clamp01(stats.health / 100f);
            shieldBars[i].fillAmount = Mathf.Clamp01(stats.stamina / 100f);

            int index = i;
            button.onClick.RemoveAllListeners();
            button.onClick.AddListener(() => SelectDisplayedPerson(index));
        }

        for (int i = people.Length; i < unitButtons.Count; i++)
        {
            unitButtons[i].gameObject.SetActive(false);
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
        Canvas canvas = new GameObject("Unit List Canvas").AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 999;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Unit List Window");
        panel.transform.SetParent(canvas.transform, false);

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

        Text subtitle = CreateText(contentRoot, "\uC720\uB2DB \uC0C1\uD0DC\uC640 \uD604\uC7AC \uBA85\uB839", 16, TextAnchor.MiddleLeft);
        RectTransform subtitleRect = subtitle.GetComponent<RectTransform>();
        subtitleRect.anchorMin = new Vector2(0f, 1f);
        subtitleRect.anchorMax = new Vector2(1f, 1f);
        subtitleRect.offsetMin = new Vector2(24f, -92f);
        subtitleRect.offsetMax = new Vector2(-24f, -58f);

        CreateHeader(contentRoot);

        GameObject listObject = new GameObject("Unit Table Rows");
        listObject.transform.SetParent(contentRoot, false);
        listRoot = listObject.AddComponent<RectTransform>();
        listRoot.anchorMin = new Vector2(0f, 0f);
        listRoot.anchorMax = new Vector2(1f, 1f);
        listRoot.offsetMin = new Vector2(24f, 18f);
        listRoot.offsetMax = new Vector2(-24f, -136f);

        VerticalLayoutGroup layout = listObject.AddComponent<VerticalLayoutGroup>();
        layout.spacing = 0f;
        layout.childControlHeight = true;
        layout.childControlWidth = true;
        layout.childForceExpandHeight = false;
        layout.childForceExpandWidth = true;

        controls.Initialize(panelRect, contentRoot);
        controls.CreateButton("-", new Vector2(-76f, -10f), controls.ToggleMinimize);
        controls.CreateButton("\u25A1", new Vector2(-42f, -10f), controls.ToggleMaximize);
        controls.CreateButton("x", new Vector2(-8f, -10f), controls.Close);
        controls.CreateResizeHandle(new Vector2(620f, 220f));
    }

    private void EnsureButtonCount(int count)
    {
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

        CreateCell(buttonObject.transform, string.Empty, 0f, 0.10f, 17);
        CreateCell(buttonObject.transform, string.Empty, 0.10f, 0.31f, 17);
        CreateCell(buttonObject.transform, string.Empty, 0.31f, 0.48f, 17);
        CreateCell(buttonObject.transform, string.Empty, 0.48f, 0.70f, 17);
        healthBars.Add(CreateBar(buttonObject.transform, 0.70f, 0.85f, new Color(0.36f, 0.82f, 0.42f, 1f)));
        shieldBars.Add(CreateBar(buttonObject.transform, 0.85f, 1f, new Color(0.35f, 0.62f, 1f, 1f)));

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

        CreateCell(header.transform, "\uC544\uC774\uCF58", 0f, 0.10f, 17);
        CreateCell(header.transform, "\uC774\uB984", 0.10f, 0.31f, 17);
        CreateCell(header.transform, "\uC0C1\uD0DC", 0.31f, 0.48f, 17);
        CreateCell(header.transform, "\uD604\uC7AC \uBA85\uB839", 0.48f, 0.70f, 17);
        CreateCell(header.transform, "\uCCB4\uB825", 0.70f, 0.85f, 17);
        CreateCell(header.transform, "\uC2E4\uB4DC", 0.85f, 1f, 17);
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

    private Image CreateBar(Transform parent, float xMin, float xMax, Color fillColor)
    {
        GameObject backgroundObject = new GameObject("Bar Background");
        backgroundObject.transform.SetParent(parent, false);

        RectTransform backgroundRect = backgroundObject.AddComponent<RectTransform>();
        backgroundRect.anchorMin = new Vector2(xMin, 0.5f);
        backgroundRect.anchorMax = new Vector2(xMax, 0.5f);
        backgroundRect.offsetMin = new Vector2(8f, -6f);
        backgroundRect.offsetMax = new Vector2(-8f, 6f);

        Image background = backgroundObject.AddComponent<Image>();
        background.color = new Color(0.20f, 0.20f, 0.20f, 1f);

        GameObject fillObject = new GameObject("Bar Fill");
        fillObject.transform.SetParent(backgroundObject.transform, false);

        RectTransform fillRect = fillObject.AddComponent<RectTransform>();
        fillRect.anchorMin = Vector2.zero;
        fillRect.anchorMax = Vector2.one;
        fillRect.offsetMin = Vector2.zero;
        fillRect.offsetMax = Vector2.zero;

        Image fill = fillObject.AddComponent<Image>();
        fill.color = fillColor;
        fill.type = Image.Type.Filled;
        fill.fillMethod = Image.FillMethod.Horizontal;
        fill.fillOrigin = 0;
        fill.fillAmount = 1f;
        return fill;
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
        PersonMover mover = person.GetComponent<PersonMover>();
        if (mover != null && mover.IsMoving)
        {
            return "\uC774\uB3D9 \uC911";
        }

        return string.IsNullOrWhiteSpace(person.CurrentState) || person.CurrentState == "Idle"
            ? "\uB300\uAE30"
            : person.CurrentState;
    }

    private static string GetDisplayAction(PersonComponent person)
    {
        return string.IsNullOrWhiteSpace(person.CurrentAction) || person.CurrentAction == "None"
            ? "-"
            : person.CurrentAction;
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
