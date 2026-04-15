using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public enum UnitActionMode
{
    Move,
    Gather,
    Run,
    Stop
}

public class ActionWindow : MonoBehaviour
{
    public static ActionWindow Instance { get; private set; }
    public static UnitActionMode CurrentAction => FindSelectedPerson() != null ? FindSelectedPerson().ActionMode : UnitActionMode.Move;
    public static bool RunEnabled => FindSelectedPerson() != null && FindSelectedPerson().RunEnabled;

    private Font font;
    private Button moveButton;
    private Button gatherButton;
    private Button runButton;
    private Button stopButton;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<ActionWindow>() != null)
        {
            return;
        }

        GameObject windowObject = new GameObject("Action Window");
        windowObject.AddComponent<ActionWindow>();
    }

    private void Awake()
    {
        Instance = this;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        RefreshForSelectedPerson();
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Action Window Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1210;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Action Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(0.5f, 0f);
        panelRect.anchorMax = new Vector2(0.5f, 0f);
        panelRect.pivot = new Vector2(0.5f, 0f);
        panelRect.anchoredPosition = new Vector2(0f, 12f);
        panelRect.sizeDelta = new Vector2(520f, 64f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.07f, 0.07f, 0.07f, 0.92f);

        HorizontalLayoutGroup layout = panel.AddComponent<HorizontalLayoutGroup>();
        layout.spacing = 8f;
        layout.padding = new RectOffset(10, 10, 8, 8);
        layout.childControlHeight = true;
        layout.childControlWidth = true;
        layout.childForceExpandHeight = true;
        layout.childForceExpandWidth = true;

        gatherButton = CreateButton(panel.transform, "\uCC44\uC9D1", UnitActionMode.Gather);
        moveButton = CreateButton(panel.transform, "\uC774\uB3D9", UnitActionMode.Move);
        runButton = CreateButton(panel.transform, "\uB2EC\uB9AC\uAE30", UnitActionMode.Run);
        stopButton = CreateButton(panel.transform, "\uBA48\uCD94\uAE30", UnitActionMode.Stop);
    }

    private Button CreateButton(Transform parent, string label, UnitActionMode action)
    {
        GameObject buttonObject = new GameObject(label);
        buttonObject.transform.SetParent(parent, false);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.12f, 0.12f, 0.12f, 0.95f);

        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(() => SelectAction(action));

        GameObject labelObject = new GameObject("Label");
        labelObject.transform.SetParent(buttonObject.transform, false);

        RectTransform labelRect = labelObject.AddComponent<RectTransform>();
        labelRect.anchorMin = Vector2.zero;
        labelRect.anchorMax = Vector2.one;
        labelRect.offsetMin = Vector2.zero;
        labelRect.offsetMax = Vector2.zero;

        Text text = labelObject.AddComponent<Text>();
        text.font = font;
        text.fontSize = 20;
        text.alignment = TextAnchor.MiddleCenter;
        text.color = Color.white;
        text.text = label;
        return button;
    }

    private void SelectAction(UnitActionMode action)
    {
        PersonComponent person = FindSelectedPerson();
        if (person == null)
        {
            RefreshForSelectedPerson();
            return;
        }

        if (action == UnitActionMode.Run)
        {
            ToggleRun(person);
            return;
        }

        person.SetActionMode(action);
        RefreshForSelectedPerson();

        if (action == UnitActionMode.Stop)
        {
            StopSelectedPerson(person);
        }
    }

    public void RefreshForSelectedPerson()
    {
        PersonComponent person = FindSelectedPerson();
        UnitActionMode action = person != null ? person.ActionMode : UnitActionMode.Move;
        bool runEnabled = person != null && person.RunEnabled;
        SetButtonColor(moveButton, action == UnitActionMode.Move);
        SetButtonColor(gatherButton, action == UnitActionMode.Gather);
        SetButtonColor(runButton, runEnabled);
        SetButtonColor(stopButton, action == UnitActionMode.Stop);
    }

    private static void SetButtonColor(Button button, bool selected)
    {
        if (button == null)
        {
            return;
        }

        button.image.color = selected
            ? new Color(0.28f, 0.40f, 0.22f, 0.98f)
            : new Color(0.12f, 0.12f, 0.12f, 0.95f);
    }

    private static void StopSelectedPerson(PersonComponent person)
    {
        MovementCommandService.TryStop(person);
        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }
    }

    private void ToggleRun(PersonComponent person)
    {
        person.SetRunEnabled(!person.RunEnabled);
        RefreshForSelectedPerson();
        ApplyRunToggleToSelectedPerson(person);
    }

    private static void ApplyRunToggleToSelectedPerson(PersonComponent person)
    {
        PersonMover mover = person.GetComponent<PersonMover>();
        if (mover != null)
        {
            mover.SetRunningForCurrentMove(person.RunEnabled);
        }

        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }
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
