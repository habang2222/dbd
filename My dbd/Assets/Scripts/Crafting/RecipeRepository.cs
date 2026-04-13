using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Dbd.Crafting
{
    [CreateAssetMenu(menuName = "DBD/Crafting/Recipe Repository")]
    public class RecipeRepository : ScriptableObject
    {
        [SerializeField] private List<RecipeData> recipes = new();

        private Dictionary<string, RecipeData> recipeById;

        public bool TryGetRecipe(string recipeId, out RecipeData recipe)
        {
            EnsureCache();
            return recipeById.TryGetValue(recipeId, out recipe);
        }

        public IReadOnlyList<RecipeData> GetRecipesByStation(string stationId)
        {
            return recipes
                .Where(recipe => recipe != null && recipe.RequiredStation == stationId)
                .ToList();
        }

        public IReadOnlyList<RecipeData> GetUnlockedRecipes(CraftContext context)
        {
            return recipes
                .Where(recipe => recipe != null && context != null && context.IsRecipeUnlocked(recipe.RecipeId))
                .ToList();
        }

        private void EnsureCache()
        {
            if (recipeById != null)
            {
                return;
            }

            recipeById = new Dictionary<string, RecipeData>();
            foreach (RecipeData recipe in recipes)
            {
                if (recipe == null || string.IsNullOrWhiteSpace(recipe.RecipeId))
                {
                    continue;
                }

                recipeById[recipe.RecipeId] = recipe;
            }
        }

        private void OnValidate()
        {
            recipeById = null;
        }
    }
}
