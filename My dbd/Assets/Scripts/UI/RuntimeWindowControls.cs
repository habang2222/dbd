using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class RuntimeWindowControls : MonoBehaviour, IDragHandler
{
    [SerializeField] private RectTransform targetWindow;
    [SerializeField] private RectTransform contentRoot;

    private Vector2 normalSize;
    private bool isMinimized;
    private bool isMaximized;

    public void Initialize(RectTransform window, RectTransform content)
    {
        targetWindow = window;
        contentRoot = content;
        normalSize = window.sizeDelta;
    }

    public void OnDrag(PointerEventData eventData)
    {
        if (targetWindow == null || isMaximized)
        {
            return;
        }

        targetWindow.anchoredPosition += eventData.delta;
    }

    public void ToggleMinimize()
    {
        if (targetWindow == null)
        {
            return;
        }

        isMinimized = !isMinimized;
        if (contentRoot != null)
        {
            contentRoot.gameObject.SetActive(!isMinimized);
        }

        targetWindow.sizeDelta = isMinimized
            ? new Vector2(normalSize.x, 58f)
            : normalSize;
    }

    public void ToggleMaximize()
    {
        if (targetWindow == null)
        {
            return;
        }

        isMaximized = !isMaximized;
        if (isMaximized)
        {
            normalSize = targetWindow.sizeDelta;
            targetWindow.anchorMin = new Vector2(0f, 0f);
            targetWindow.anchorMax = new Vector2(1f, 1f);
            targetWindow.offsetMin = new Vector2(18f, 18f);
            targetWindow.offsetMax = new Vector2(-18f, -76f);
            return;
        }

        targetWindow.anchorMin = new Vector2(0f, 1f);
        targetWindow.anchorMax = new Vector2(0f, 1f);
        targetWindow.sizeDelta = normalSize;
    }

    public void Close()
    {
        if (targetWindow != null)
        {
            targetWindow.gameObject.SetActive(false);
        }
    }

    public Image CreateButton(string label, Vector2 anchoredPosition, UnityEngine.Events.UnityAction action)
    {
        GameObject buttonObject = new GameObject(label);
        buttonObject.transform.SetParent(targetWindow, false);

        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(1f, 1f);
        rect.anchorMax = new Vector2(1f, 1f);
        rect.pivot = new Vector2(1f, 1f);
        rect.anchoredPosition = anchoredPosition;
        rect.sizeDelta = new Vector2(32f, 28f);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.14f, 0.14f, 0.14f, 0.96f);

        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(action);

        Text text = CreateLabel(buttonObject.transform, label, 18);
        text.alignment = TextAnchor.MiddleCenter;
        return image;
    }

    public void CreateResizeHandle(Vector2 minSize)
    {
        GameObject handle = new GameObject("Resize Handle");
        handle.transform.SetParent(targetWindow, false);

        RectTransform rect = handle.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(1f, 0f);
        rect.anchorMax = new Vector2(1f, 0f);
        rect.pivot = new Vector2(1f, 0f);
        rect.anchoredPosition = new Vector2(-8f, 8f);
        rect.sizeDelta = new Vector2(26f, 26f);

        Image image = handle.AddComponent<Image>();
        image.color = new Color(0.28f, 0.28f, 0.28f, 0.85f);
        image.raycastTarget = true;

        WindowResizeHandle resize = handle.AddComponent<WindowResizeHandle>();
        resize.Initialize(targetWindow, minSize);
    }

    private Text CreateLabel(Transform parent, string value, int size)
    {
        GameObject labelObject = new GameObject("Label");
        labelObject.transform.SetParent(parent, false);

        RectTransform rect = labelObject.AddComponent<RectTransform>();
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.offsetMin = Vector2.zero;
        rect.offsetMax = Vector2.zero;

        Text text = labelObject.AddComponent<Text>();
        text.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        text.fontSize = size;
        text.color = Color.white;
        text.text = value;
        return text;
    }
}
