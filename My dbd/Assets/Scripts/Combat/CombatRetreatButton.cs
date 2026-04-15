using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class CombatRetreatButton : MonoBehaviour
{
    private GameObject buttonObject;
    private Font font;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<CombatRetreatButton>() != null)
        {
            return;
        }

        GameObject retreatObject = new GameObject("Combat Retreat Button");
        retreatObject.AddComponent<CombatRetreatButton>();
    }

    private void Awake()
    {
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        CreateUi();
        buttonObject.SetActive(false);
    }

    private void Update()
    {
        buttonObject.SetActive(UnitCombatController.HasActiveCombat);
    }

    private void CreateUi()
    {
        Canvas canvas = new GameObject("Combat Retreat Canvas").AddComponent<Canvas>();
        canvas.transform.SetParent(transform, false);
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 1300;
        canvas.gameObject.AddComponent<CanvasScaler>();
        canvas.gameObject.AddComponent<GraphicRaycaster>();
        EnsureEventSystem();

        buttonObject = new GameObject("Retreat Button");
        buttonObject.transform.SetParent(canvas.transform, false);

        RectTransform rect = buttonObject.AddComponent<RectTransform>();
        rect.anchorMin = new Vector2(0.5f, 0f);
        rect.anchorMax = new Vector2(0.5f, 0f);
        rect.pivot = new Vector2(0.5f, 0f);
        rect.anchoredPosition = new Vector2(0f, 36f);
        rect.sizeDelta = new Vector2(180f, 56f);

        Image image = buttonObject.AddComponent<Image>();
        image.color = new Color(0.72f, 0.14f, 0.12f, 0.95f);

        Button button = buttonObject.AddComponent<Button>();
        button.onClick.AddListener(OnRetreatClicked);

        GameObject labelObject = new GameObject("Label");
        labelObject.transform.SetParent(buttonObject.transform, false);

        RectTransform labelRect = labelObject.AddComponent<RectTransform>();
        labelRect.anchorMin = Vector2.zero;
        labelRect.anchorMax = Vector2.one;
        labelRect.offsetMin = Vector2.zero;
        labelRect.offsetMax = Vector2.zero;

        Text label = labelObject.AddComponent<Text>();
        label.font = font;
        label.fontSize = 24;
        label.alignment = TextAnchor.MiddleCenter;
        label.color = Color.white;
        label.text = "\uD6C4\uD1F4";
    }

    private void OnRetreatClicked()
    {
        UnitCombatController.Retreat();
        buttonObject.SetActive(false);
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
