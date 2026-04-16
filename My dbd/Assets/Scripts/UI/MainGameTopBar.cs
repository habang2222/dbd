using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class MainGameTopBar : MonoBehaviour
{
    public static MainGameTopBar Instance { get; private set; }

    private readonly List<Button> menuButtons = new();
    private readonly string[] tabLabels =
    {
        "\uBA54\uB274",
        "\uC18C\uC720\uD55C \uBB3C\uAC74",
        "\uCEE4\uBBA4\uB2C8\uD2F0",
        "\uCCAD\uC0AC\uC9C4",
        "\uC720\uB2DB \uBAA9\uB85D",
        "\uBBF8\uC815",
        "\uCC3D",
        "\uC124\uC815"
    };
    private readonly string[] directorTabLabels =
    {
        "\uC0DD\uC131",
        "\uC9C0\uD615",
        "\uBBF8\uC815"
    };

    private Font font;
    private int selectedIndex;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<MainGameTopBar>() != null)
        {
            return;
        }

        GameObject topBarObject = new GameObject("Main Game Top Bar");
        topBarObject.AddComponent<MainGameTopBar>();
    }

    private void Awake()
    {
        Instance = this;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        RefreshForRole();
        SelectTab(0);
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
        Canvas canvas = new GameObject("Main Game UI Canvas").AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1200;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject bar = new GameObject("Top Menu Bar");
        bar.transform.SetParent(canvas.transform, false);

        RectTransform barRect = bar.AddComponent<RectTransform>();
        barRect.anchorMin = new Vector2(0f, 1f);
        barRect.anchorMax = new Vector2(1f, 1f);
        barRect.pivot = new Vector2(0.5f, 1f);
        barRect.offsetMin = new Vector2(0f, -58f);
        barRect.offsetMax = new Vector2(0f, 0f);

        Image background = bar.AddComponent<Image>();
        background.color = new Color(0.05f, 0.05f, 0.05f, 0.92f);

        HorizontalLayoutGroup layout = bar.AddComponent<HorizontalLayoutGroup>();
        layout.spacing = 0f;
        layout.padding = new RectOffset(0, 0, 0, 0);
        layout.childControlHeight = true;
        layout.childControlWidth = true;
        layout.childForceExpandHeight = true;
        layout.childForceExpandWidth = true;

        for (int i = 0; i < tabLabels.Length; i++)
        {
            int index = i;
            Button button = CreateTabButton(bar.transform, tabLabels[i]);
            button.onClick.AddListener(() => SelectTab(index));
            menuButtons.Add(button);
        }
    }

    public void RefreshForRole()
    {
        string[] labels = SessionRoleService.IsDirector ? directorTabLabels : tabLabels;
        for (int i = 0; i < menuButtons.Count; i++)
        {
            bool active = i < labels.Length;
            menuButtons[i].gameObject.SetActive(active);
            if (!active)
            {
                continue;
            }

            Text label = menuButtons[i].GetComponentInChildren<Text>();
            if (label != null)
            {
                label.text = labels[i];
            }
        }

        if (selectedIndex >= labels.Length)
        {
            selectedIndex = 0;
        }
    }

    private Button CreateTabButton(Transform parent, string labelText)
    {
        GameObject buttonObject = new GameObject("Top Menu Tab");
        buttonObject.transform.SetParent(parent, false);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.09f, 0.09f, 0.09f, 0.95f);

        Button button = buttonObject.AddComponent<Button>();

        LayoutElement layout = buttonObject.AddComponent<LayoutElement>();
        layout.minWidth = 110f;
        layout.preferredWidth = 150f;

        GameObject labelObject = new GameObject("Label");
        labelObject.transform.SetParent(buttonObject.transform, false);

        RectTransform labelRect = labelObject.AddComponent<RectTransform>();
        labelRect.anchorMin = Vector2.zero;
        labelRect.anchorMax = Vector2.one;
        labelRect.offsetMin = new Vector2(8f, 0f);
        labelRect.offsetMax = new Vector2(-8f, 0f);

        Text label = labelObject.AddComponent<Text>();
        label.font = font;
        label.fontSize = 24;
        label.alignment = TextAnchor.MiddleCenter;
        label.color = new Color(0.92f, 0.92f, 0.88f, 1f);
        label.horizontalOverflow = HorizontalWrapMode.Wrap;
        label.verticalOverflow = VerticalWrapMode.Truncate;
        label.text = labelText;

        return button;
    }

    private void SelectTab(int index)
    {
        if (SessionRoleService.IsDirector)
        {
            SelectDirectorTab(index);
            return;
        }

        selectedIndex = index;
        for (int i = 0; i < menuButtons.Count; i++)
        {
            Image image = menuButtons[i].image;
            image.color = i == selectedIndex
                ? new Color(0.20f, 0.20f, 0.18f, 0.98f)
                : new Color(0.09f, 0.09f, 0.09f, 0.95f);
        }

        if (index == 1 && OwnedItemsWindow.Instance != null)
        {
            OwnedItemsWindow.Instance.Toggle();
        }

        if (index == 3 && Dbd.Crafting.BlueprintWindowController.Instance != null)
        {
            Dbd.Crafting.BlueprintWindowController.Instance.Toggle();
        }
        else if (index == 3)
        {
            GameObject blueprintWindow = new GameObject("Blueprint Window");
            blueprintWindow.AddComponent<Dbd.Crafting.BlueprintWindowController>();
            Dbd.Crafting.BlueprintWindowController.Instance.Show();
        }

        if (index == 4 && UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.Toggle();
        }
        else if (index == 4)
        {
            GameObject unitListPanel = new GameObject("Unit List Panel");
            unitListPanel.AddComponent<UnitListPanel>();
            UnitListPanel.Instance.Show();
        }

        if (index == 6 && WindowMenuPanel.Instance != null)
        {
            WindowMenuPanel.Instance.Toggle();
        }
        else if (index == 6)
        {
            GameObject windowMenuPanel = new GameObject("Window Menu Panel");
            windowMenuPanel.AddComponent<WindowMenuPanel>();
            WindowMenuPanel.Instance.Show();
        }
    }

    private void SelectDirectorTab(int index)
    {
        selectedIndex = Mathf.Clamp(index, 0, directorTabLabels.Length - 1);
        for (int i = 0; i < menuButtons.Count; i++)
        {
            if (!menuButtons[i].gameObject.activeSelf)
            {
                continue;
            }

            Image image = menuButtons[i].image;
            image.color = i == selectedIndex
                ? new Color(0.20f, 0.20f, 0.18f, 0.98f)
                : new Color(0.09f, 0.09f, 0.09f, 0.95f);
        }

        if (DirectorWorldTool.Instance == null)
        {
            return;
        }

        if (selectedIndex == 0)
        {
            ShowDirectorCreationWindow();
        }
        else if (selectedIndex == 1)
        {
            ShowDirectorTerrainWindow();
        }
        else
        {
            HideDirectorWindows();
            DirectorWorldTool.Instance.SetMode(DirectorToolMode.None);
        }
    }

    private static void ShowDirectorCreationWindow()
    {
        if (DirectorCreationWindow.Instance != null)
        {
            DirectorCreationWindow.Instance.Show();
        }

        if (DirectorTerrainWindow.Instance != null)
        {
            DirectorTerrainWindow.Instance.Hide();
        }
    }

    private static void ShowDirectorTerrainWindow()
    {
        if (DirectorTerrainWindow.Instance != null)
        {
            DirectorTerrainWindow.Instance.Show();
        }

        if (DirectorCreationWindow.Instance != null)
        {
            DirectorCreationWindow.Instance.Hide();
        }
    }

    private static void HideDirectorWindows()
    {
        if (DirectorCreationWindow.Instance != null)
        {
            DirectorCreationWindow.Instance.Hide();
        }

        if (DirectorTerrainWindow.Instance != null)
        {
            DirectorTerrainWindow.Instance.Hide();
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
