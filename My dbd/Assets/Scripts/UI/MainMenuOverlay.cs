using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class MainMenuOverlay : MonoBehaviour
{
    private const float GameTimeScale = 1f;
    private static bool hasStarted;

    private Canvas canvas;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (hasStarted || FindFirstObjectByType<MainMenuOverlay>() != null)
        {
            return;
        }

        GameObject menu = new GameObject("Main Menu Overlay");
        menu.AddComponent<MainMenuOverlay>();
    }

    private void Awake()
    {
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        EnsureEventSystem();
        CreateUi();
        Time.timeScale = 0f;
    }

    private void OnDestroy()
    {
        if (hasStarted && Mathf.Approximately(Time.timeScale, 0f))
        {
            Time.timeScale = GameTimeScale;
        }
    }

    private void CreateUi()
    {
        canvas = new GameObject("Main Menu Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 5000;

        CanvasScaler scaler = canvas.gameObject.AddComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920f, 1080f);
        scaler.matchWidthOrHeight = 0.5f;

        canvas.gameObject.AddComponent<GraphicRaycaster>();

        GameObject blocker = new GameObject("Main Menu Blocker");
        blocker.transform.SetParent(canvas.transform, false);

        RectTransform blockerRect = blocker.AddComponent<RectTransform>();
        blockerRect.anchorMin = Vector2.zero;
        blockerRect.anchorMax = Vector2.one;
        blockerRect.offsetMin = Vector2.zero;
        blockerRect.offsetMax = Vector2.zero;

        Image blockerImage = blocker.AddComponent<Image>();
        blockerImage.color = new Color(0.04f, 0.05f, 0.04f, 0.96f);
        blockerImage.raycastTarget = true;

        Button playButton = CreatePlayButton(blocker.transform);
        playButton.onClick.AddListener(StartGame);
    }

    private Button CreatePlayButton(Transform parent)
    {
        GameObject buttonObject = new GameObject("Play Button");
        buttonObject.transform.SetParent(parent, false);

        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0.5f, 0.5f);
        rect.anchorMax = new Vector2(0.5f, 0.5f);
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.anchoredPosition = Vector2.zero;
        rect.sizeDelta = new Vector2(320f, 84f);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.18f, 0.43f, 0.24f, 1f);

        Button button = buttonObject.AddComponent<Button>();
        ColorBlock colors = button.colors;
        colors.normalColor = new Color(0.18f, 0.43f, 0.24f, 1f);
        colors.highlightedColor = new Color(0.25f, 0.55f, 0.32f, 1f);
        colors.pressedColor = new Color(0.12f, 0.32f, 0.17f, 1f);
        colors.selectedColor = colors.highlightedColor;
        button.colors = colors;

        GameObject labelObject = new GameObject("Label");
        labelObject.transform.SetParent(buttonObject.transform, false);

        RectTransform labelRect = labelObject.AddComponent<RectTransform>();
        labelRect.anchorMin = Vector2.zero;
        labelRect.anchorMax = Vector2.one;
        labelRect.offsetMin = Vector2.zero;
        labelRect.offsetMax = Vector2.zero;

        Text label = labelObject.AddComponent<Text>();
        label.font = font;
        label.text = "\uD50C\uB808\uC774 \uD558\uAE30";
        label.fontSize = 36;
        label.alignment = TextAnchor.MiddleCenter;
        label.color = Color.white;

        return button;
    }

    private void StartGame()
    {
        hasStarted = true;
        Time.timeScale = GameTimeScale;
        Destroy(gameObject);
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
