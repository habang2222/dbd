using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class MainMenuOverlay : MonoBehaviour
{
    private const float GameTimeScale = 1f;
    private static bool hasStarted;

    private Canvas canvas;
    private Font font;
    private InputField nicknameInput;

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

        CreateTitle(blocker.transform);
        nicknameInput = CreateNicknameInput(blocker.transform);
        Button playButton = CreatePlayButton(blocker.transform);
        playButton.onClick.AddListener(StartGame);
    }

    private void CreateTitle(Transform parent)
    {
        GameObject titleObject = new GameObject("Title");
        titleObject.transform.SetParent(parent, false);

        RectTransform rect = titleObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0.5f, 0.5f);
        rect.anchorMax = new Vector2(0.5f, 0.5f);
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.anchoredPosition = new Vector2(0f, 150f);
        rect.sizeDelta = new Vector2(520f, 72f);

        Text title = titleObject.AddComponent<Text>();
        title.font = font;
        title.text = "닉네임을 입력하세요";
        title.fontSize = 36;
        title.alignment = TextAnchor.MiddleCenter;
        title.color = Color.white;
    }

    private InputField CreateNicknameInput(Transform parent)
    {
        GameObject inputObject = new GameObject("Nickname Input");
        inputObject.transform.SetParent(parent, false);

        RectTransform rect = inputObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0.5f, 0.5f);
        rect.anchorMax = new Vector2(0.5f, 0.5f);
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.anchoredPosition = new Vector2(0f, 62f);
        rect.sizeDelta = new Vector2(420f, 58f);

        Image image = inputObject.AddComponent<Image>();
        image.color = new Color(0.12f, 0.14f, 0.13f, 1f);

        InputField input = inputObject.AddComponent<InputField>();
        input.characterLimit = 18;
        input.contentType = InputField.ContentType.Standard;
        input.text = PlayerProfileService.HasNickname ? PlayerProfileService.LocalNickname : string.Empty;

        Text text = CreateInputText(inputObject.transform, "Text", string.Empty, TextAnchor.MiddleLeft, Color.white);
        text.rectTransform.offsetMin = new Vector2(18f, 0f);
        text.rectTransform.offsetMax = new Vector2(-18f, 0f);
        input.textComponent = text;

        Text placeholder = CreateInputText(inputObject.transform, "Placeholder", "Unknown1", TextAnchor.MiddleLeft, new Color(0.68f, 0.72f, 0.69f, 1f));
        placeholder.rectTransform.offsetMin = new Vector2(18f, 0f);
        placeholder.rectTransform.offsetMax = new Vector2(-18f, 0f);
        input.placeholder = placeholder;

        return input;
    }

    private Text CreateInputText(Transform parent, string name, string value, TextAnchor alignment, Color color)
    {
        GameObject textObject = new GameObject(name);
        textObject.transform.SetParent(parent, false);
        RectTransform rect = textObject.AddComponent<RectTransform>();
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;

        Text text = textObject.AddComponent<Text>();
        text.font = font;
        text.fontSize = 24;
        text.alignment = alignment;
        text.color = color;
        text.text = value;
        return text;
    }

    private Button CreatePlayButton(Transform parent)
    {
        GameObject buttonObject = new GameObject("Play Button");
        buttonObject.transform.SetParent(parent, false);

        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0.5f, 0.5f);
        rect.anchorMax = new Vector2(0.5f, 0.5f);
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.anchoredPosition = new Vector2(0f, -28f);
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
        PlayerProfileService.SetLocalNickname(nicknameInput != null ? nicknameInput.text : string.Empty);
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
