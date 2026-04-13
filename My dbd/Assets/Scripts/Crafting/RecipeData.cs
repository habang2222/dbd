using System.Collections.Generic;
using Dbd.Items;
using UnityEngine;

namespace Dbd.Crafting
{
    [CreateAssetMenu(menuName = "DBD/Crafting/Recipe Data")]
    public class RecipeData : ScriptableObject
    {
        [SerializeField] private string recipeId;
        [SerializeField] private List<ItemStack> inputs = new();
        [SerializeField] private List<ItemStack> outputs = new();
        [SerializeField] private float craftTime;
        [SerializeField] private string requiredStation;
        [SerializeField] private List<RecipeRequirement> requirements = new();
        [SerializeField] private List<string> tags = new();

        public string RecipeId => recipeId;
        public IReadOnlyList<ItemStack> Inputs => inputs;
        public IReadOnlyList<ItemStack> Outputs => outputs;
        public float CraftTime => Mathf.Max(0f, craftTime);
        public string RequiredStation => requiredStation;
        public IReadOnlyList<RecipeRequirement> Requirements => requirements;
        public IReadOnlyList<string> Tags => tags;

        public void ConfigureForRuntime(
            string nextRecipeId,
            IEnumerable<ItemStack> nextInputs,
            IEnumerable<ItemStack> nextOutputs,
            float nextCraftTime = 0f,
            string nextRequiredStation = "")
        {
            recipeId = nextRecipeId;
            inputs = new List<ItemStack>(nextInputs);
            outputs = new List<ItemStack>(nextOutputs);
            craftTime = nextCraftTime;
            requiredStation = nextRequiredStation;
        }
    }
}
