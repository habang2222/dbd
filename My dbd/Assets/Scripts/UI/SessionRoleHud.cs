using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class SessionRoleHud : MonoBehaviour
{
    private Text roleText;
    private Button playerButton;
    private Button directorButton;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<SessionRoleHud>() != null)
        {
            return;
        }

        GameObject hudObject = new GameObject("Session Role HUD");
        hudObject.AddComponent<SessionRoleHud>();
    }

    private void Awake()
    {
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        SessionRoleService.ApplyDefaultOwnership();
        Refresh();
    }

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.F9))
        {
            SessionRoleService.SetRole(SessionRole.Player);
        }

        if (Input.GetKeyDown(KeyCode.F10))
        {
            SessionRoleService.SetRole(SessionRole.Director);
        }
    }

    public void Refresh()
    {
        if (roleText != null)
        {
            roleText.text = $"Mode: {SessionRoleService.GetRoleName()}";
        }

        if (playerButton != null)
        {
            playerButton.image.color = SessionRoleService.IsPlayer
                ? new Color(0.18f, 0.32f, 0.18f, 0.98f)
                : new Color(0.12f, 0.12f, 0.12f, 0.96f);
        }

        if (directorButton != null)
        {
            directorButton.image.color = SessionRoleService.IsDirector
                ? new Color(0.34f, 0.24f, 0.12f, 0.98f)
                : new Color(0.12f, 0.12f, 0.12f, 0.96f);
        }
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Session Role Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1300;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = new GameObject("Session Role Panel");
        panel.transform.SetParent(canvas.transform, false);

        RectTransform panelRect = panel.AddComponent<RectTransform>();
        panelRect.anchorMin = new Vector2(1f, 1f);
        panelRect.anchorMax = new Vector2(1f, 1f);
        panelRect.pivot = new Vector2(1f, 1f);
        panelRect.anchoredPosition = new Vector2(-18f, -64f);
        panelRect.sizeDelta = new Vector2(330f, 54f);

        Image background = panel.AddComponent<Image>();
        background.color = new Color(0.05f, 0.05f, 0.05f, 0.82f);

        HorizontalLayoutGroup layout = panel.AddComponent<HorizontalLayoutGroup>();
        layout.padding = new RectOffset(8, 8, 7, 7);
        layout.spacing = 6f;
        layout.childControlHeight = true;
        layout.childControlWidth = true;
        layout.childForceExpandHeight = true;
        layout.childForceExpandWidth = false;

        roleText = CreateLabel(panel.transform, "Mode: Player", 120f);
        playerButton = CreateButton(panel.transform, "Player F9", 86f);
        directorButton = CreateButton(panel.transform, "Director F10", 106f);
        playerButton.onClick.AddListener(() => SessionRoleService.SetRole(SessionRole.Player));
        directorButton.onClick.AddListener(() => SessionRoleService.SetRole(SessionRole.Director));
    }

    private Text CreateLabel(Transform parent, string value, float width)
    {
        GameObject labelObject = new GameObject("Role Label");
        labelObject.transform.SetParent(parent, false);
        LayoutElement layout = labelObject.AddComponent<LayoutElement>();
        layout.preferredWidth = width;

        Text text = labelObject.AddComponent<Text>();
        text.font = font;
        text.fontSize = 17;
        text.alignment = TextAnchor.MiddleCenter;
        text.color = Color.white;
        text.text = value;
        return text;
    }

    private Button CreateButton(Transform parent, string value, float width)
    {
        GameObject buttonObject = new GameObject(value);
        buttonObject.transform.SetParent(parent, false);
        LayoutElement layout = buttonObject.AddComponent<LayoutElement>();
        layout.preferredWidth = width;

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.12f, 0.12f, 0.12f, 0.96f);
        Button button = buttonObject.AddComponent<Button>();

        Text text = CreateLabel(buttonObject.transform, value, width);
        RectTransform textRect = text.rectTransform;
        textRect.anchorMin = Vector2.zero;
        textRect.anchorMax = Vector2.one;
        textRect.offsetMin = Vector2.zero;
        textRect.offsetMax = Vector2.zero;
        return button;
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
