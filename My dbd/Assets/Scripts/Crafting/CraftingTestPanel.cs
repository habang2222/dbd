using System.Collections.Generic;
using Dbd.Items;
using UnityEngine;
using UnityEngine.UI;

namespace Dbd.Crafting
{
    public class CraftingTestPanel : MonoBehaviour
    {
        private const string WoodId = "wood1";
        private const string StoneId = "stone1";
        private const string ToolId = "tool1";

        private Inventory inventory;
        private RecipeData recipe;
        private CraftContext craftContext;
        private CraftingService craftingService;
        private Text statusText;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void CreateOnSceneLoad()
        {
            if (FindFirstObjectByType<CraftingTestPanel>() != null)
            {
                return;
            }

            GameObject panelObject = new GameObject("Crafting Test Panel");
            panelObject.AddComponent<CraftingTestPanel>();
        }

        private void Awake()
        {
            craftingService = new CraftingService();
            craftContext = new CraftContext
            {
                CrafterEntityId = "test_entity"
            };

            inventory = new Inventory(8, null);
            inventory.AddItems(new[]
            {
                new ItemStack(WoodId, 2),
                new ItemStack(StoneId, 1)
            });

            recipe = ScriptableObject.CreateInstance<RecipeData>();
            recipe.ConfigureForRuntime(
                "tool1_from_wood1_stone1",
                new[] { new ItemStack(WoodId, 2), new ItemStack(StoneId, 1) },
                new[] { new ItemStack(ToolId, 1) });

            CreateUi();
            RefreshStatus();
        }

        private void CreateUi()
        {
            Canvas canvas = new GameObject("Crafting Test Canvas").AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.gameObject.AddComponent<CanvasScaler>();
            canvas.gameObject.AddComponent<GraphicRaycaster>();

            Font font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");

            GameObject panel = new GameObject("Crafting Test Info");
            panel.transform.SetParent(canvas.transform, false);

            RectTransform panelRect = panel.AddComponent<RectTransform>();
            panelRect.anchorMin = new Vector2(1f, 0.5f);
            panelRect.anchorMax = new Vector2(1f, 0.5f);
            panelRect.pivot = new Vector2(1f, 0.5f);
            panelRect.anchoredPosition = new Vector2(-24f, 0f);
            panelRect.sizeDelta = new Vector2(280f, 180f);

            statusText = CreateText(panel.transform, font);

            Button craftButton = CreateButton(panel.transform, font);
            craftButton.onClick.AddListener(TryCraft);
        }

        private Text CreateText(Transform parent, Font font)
        {
            GameObject textObject = new GameObject("Inventory Text");
            textObject.transform.SetParent(parent, false);

            RectTransform rect = textObject.AddComponent<RectTransform>();
            rect.anchorMin = new Vector2(0f, 0.35f);
            rect.anchorMax = new Vector2(1f, 1f);
            rect.offsetMin = Vector2.zero;
            rect.offsetMax = Vector2.zero;

            Text text = textObject.AddComponent<Text>();
            text.font = font;
            text.fontSize = 22;
            text.alignment = TextAnchor.UpperLeft;
            text.color = Color.white;

            return text;
        }

        private Button CreateButton(Transform parent, Font font)
        {
            GameObject buttonObject = new GameObject("Craft Tool Button");
            buttonObject.transform.SetParent(parent, false);

            RectTransform rect = buttonObject.AddComponent<RectTransform>();
            rect.anchorMin = new Vector2(0f, 0f);
            rect.anchorMax = new Vector2(1f, 0.28f);
            rect.offsetMin = Vector2.zero;
            rect.offsetMax = Vector2.zero;

            Image image = buttonObject.AddComponent<Image>();
            image.color = new Color(0.18f, 0.55f, 0.26f, 0.95f);

            Button button = buttonObject.AddComponent<Button>();

            GameObject labelObject = new GameObject("Label");
            labelObject.transform.SetParent(buttonObject.transform, false);

            RectTransform labelRect = labelObject.AddComponent<RectTransform>();
            labelRect.anchorMin = Vector2.zero;
            labelRect.anchorMax = Vector2.one;
            labelRect.offsetMin = Vector2.zero;
            labelRect.offsetMax = Vector2.zero;

            Text label = labelObject.AddComponent<Text>();
            label.font = font;
            label.fontSize = 20;
            label.alignment = TextAnchor.MiddleCenter;
            label.color = Color.white;
            label.text = "Craft tool1";

            return button;
        }

        private void TryCraft()
        {
            craftingService.ExecuteInstantCraft(recipe, inventory, craftContext);
            RefreshStatus();
        }

        private void RefreshStatus()
        {
            IReadOnlyList<CraftFailReason> failureReasons = craftingService.GetCraftFailureReasons(recipe, inventory, craftContext);
            string result = failureReasons.Count == 0
                ? "Can craft: YES"
                : "Can craft: NO - " + string.Join(", ", failureReasons);

            statusText.text =
                "Test Recipe\n" +
                "wood1 x2 + stone1 x1 -> tool1 x1\n\n" +
                "Item Counts\n" +
                $"- wood1: {inventory.GetItemCount(WoodId)}\n" +
                $"- stone1: {inventory.GetItemCount(StoneId)}\n" +
                $"- tool1: {inventory.GetItemCount(ToolId)}\n\n" +
                result;
        }
    }
}
