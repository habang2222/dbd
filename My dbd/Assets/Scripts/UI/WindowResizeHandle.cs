using UnityEngine;
using UnityEngine.EventSystems;

public class WindowResizeHandle : MonoBehaviour, IDragHandler
{
    private RectTransform targetWindow;
    private Vector2 minSize;

    public void Initialize(RectTransform window, Vector2 minimumSize)
    {
        targetWindow = window;
        minSize = minimumSize;
    }

    public void OnDrag(PointerEventData eventData)
    {
        if (targetWindow == null)
        {
            return;
        }

        Canvas canvas = targetWindow.GetComponentInParent<Canvas>();
        float scaleFactor = canvas == null ? 1f : canvas.scaleFactor;
        Vector2 delta = eventData.delta / Mathf.Max(0.01f, scaleFactor);

        Vector2 size = targetWindow.sizeDelta;
        size.x = Mathf.Max(minSize.x, size.x + delta.x);
        size.y = Mathf.Max(minSize.y, size.y - delta.y);
        targetWindow.sizeDelta = size;
    }
}
