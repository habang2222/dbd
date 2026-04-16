using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public sealed class RelayConnectionWindow : MonoBehaviour
{
    public static RelayConnectionWindow Instance { get; private set; }

    private GameObject canvasObject;
    private Text statusText;
    private InputField joinCodeInput;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<RelayConnectionWindow>() != null)
        {
            return;
        }

        GameObject window = new GameObject("Relay Connection Window");
        window.AddComponent<RelayConnectionWindow>();
    }

    private void Awake()
    {
        Instance = this;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        Refresh();
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
        Refresh();
    }

    private void CreateUi()
    {
        canvasObject = new GameObject("Relay Connection Canvas");
        canvasObject.transform.SetParent(transform, false);
        Canvas canvas = canvasObject.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1330;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        GameObject panel = CreatePanel(canvas.transform);
        statusText = CreateText(panel.transform, "Relay 준비 전", 16, TextAnchor.MiddleLeft);
        statusText.rectTransform.anchorMin = new Vector2(0f, 1f);
        statusText.rectTransform.anchorMax = new Vector2(1f, 1f);
        statusText.rectTransform.offsetMin = new Vector2(14f, -64f);
        statusText.rectTransform.offsetMax = new Vector2(-14f, -34f);

        joinCodeInput = CreateInput(panel.transform, "JOIN CODE", new Vector2(14f, -108f));
        CreateButton(panel.transform, "Host Relay", new Vector2(14f, -160f), StartHost);
        CreateButton(panel.transform, "Join Relay", new Vector2(156f, -160f), JoinHost);
        CreateButton(panel.transform, "Disconnect", new Vector2(14f, -212f), Disconnect);
    }

    private GameObject CreatePanel(Transform parent)
    {
        GameObject panel = new GameObject("Relay Connection Panel");
        panel.transform.SetParent(parent, false);
        RectTransform rect = panel.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 0f);
        rect.anchorMax = new Vector2(0f, 0f);
        rect.pivot = new Vector2(0f, 0f);
        rect.anchoredPosition = new Vector2(18f, 18f);
        rect.sizeDelta = new Vector2(300f, 244f);
        panel.AddComponent<Image>().color = new Color(0.06f, 0.06f, 0.06f, 0.94f);

        Text title = CreateText(panel.transform, "Unity Relay", 22, TextAnchor.MiddleLeft);
        title.rectTransform.anchorMin = new Vector2(0f, 1f);
        title.rectTransform.anchorMax = new Vector2(1f, 1f);
        title.rectTransform.offsetMin = new Vector2(14f, -34f);
        title.rectTransform.offsetMax = new Vector2(-14f, -6f);
        return panel;
    }

    private InputField CreateInput(Transform parent, string placeholder, Vector2 position)
    {
        GameObject inputObject = new GameObject("Relay Join Code Input");
        inputObject.transform.SetParent(parent, false);
        RectTransform rect = inputObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(0f, 1f);
        rect.offsetMin = new Vector2(position.x, position.y - 38f);
        rect.offsetMax = new Vector2(position.x + 270f, position.y);
        inputObject.AddComponent<Image>().color = new Color(0.12f, 0.12f, 0.12f, 1f);

        Text text = CreateText(inputObject.transform, string.Empty, 20, TextAnchor.MiddleLeft);
        text.rectTransform.offsetMin = new Vector2(10f, 0f);
        text.rectTransform.offsetMax = new Vector2(-10f, 0f);

        Text placeholderText = CreateText(inputObject.transform, placeholder, 18, TextAnchor.MiddleLeft);
        placeholderText.rectTransform.offsetMin = new Vector2(10f, 0f);
        placeholderText.rectTransform.offsetMax = new Vector2(-10f, 0f);
        placeholderText.color = new Color(1f, 1f, 1f, 0.42f);

        InputField input = inputObject.AddComponent<InputField>();
        input.textComponent = text;
        input.placeholder = placeholderText;
        input.characterLimit = 12;
        input.contentType = InputField.ContentType.Alphanumeric;
        return input;
    }

    private void CreateButton(Transform parent, string label, Vector2 position, UnityEngine.Events.UnityAction action)
    {
        GameObject buttonObject = new GameObject(label);
        buttonObject.transform.SetParent(parent, false);
        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(0f, 1f);
        rect.offsetMin = new Vector2(position.x, position.y - 38f);
        rect.offsetMax = new Vector2(position.x + 128f, position.y);
        buttonObject.AddComponent<Image>().color = new Color(0.16f, 0.16f, 0.16f, 1f);
        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(action);

        Text text = CreateText(buttonObject.transform, label, 18, TextAnchor.MiddleCenter);
        text.rectTransform.anchorMin = Vector2.zero;
        text.rectTransform.anchorMax = Vector2.one;
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
        text.text = value;
        return text;
    }

    private async void StartHost()
    {
        if (UnityRelayConnectionService.Instance != null)
        {
            string code = await UnityRelayConnectionService.Instance.StartHostWithRelay();
            if (!string.IsNullOrEmpty(code) && joinCodeInput != null)
            {
                joinCodeInput.text = code;
            }
        }
    }

    private async void JoinHost()
    {
        if (UnityRelayConnectionService.Instance != null && joinCodeInput != null)
        {
            await UnityRelayConnectionService.Instance.StartClientWithRelay(joinCodeInput.text);
        }
    }

    private void Disconnect()
    {
        if (UnityRelayConnectionService.Instance != null)
        {
            UnityRelayConnectionService.Instance.Shutdown();
        }
    }

    private void Refresh()
    {
        if (statusText == null)
        {
            return;
        }

        UnityRelayConnectionService service = UnityRelayConnectionService.Instance;
        if (service == null)
        {
            statusText.text = "Relay 서비스 준비 중";
            return;
        }

        statusText.text = service.Status;
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
