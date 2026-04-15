using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Dbd.Crafting
{
    [CreateAssetMenu(menuName = "DBD/Crafting/Blueprint Database", fileName = "BlueprintDatabase")]
    public sealed class BlueprintDatabase : ScriptableObject
    {
        [SerializeField] private List<BlueprintItem> items = new List<BlueprintItem>();
        [SerializeField] private List<BlueprintRecipe> recipes = new List<BlueprintRecipe>();

        private readonly Dictionary<string, BlueprintItem> itemsById = new Dictionary<string, BlueprintItem>();
        private readonly Dictionary<string, List<BlueprintRecipe>> recipesByResultId = new Dictionary<string, List<BlueprintRecipe>>();
        private readonly Dictionary<string, List<BlueprintRecipe>> recipesByIngredientId = new Dictionary<string, List<BlueprintRecipe>>();
        private readonly List<BlueprintItem> cachedItems = new List<BlueprintItem>();
        private bool indexDirty = true;

        public IReadOnlyList<BlueprintItem> Items
        {
            get
            {
                EnsureIndex();
                return cachedItems;
            }
        }

        public void ReplaceAll(IEnumerable<BlueprintItem> nextItems, IEnumerable<BlueprintRecipe> nextRecipes)
        {
            items = nextItems.ToList();
            recipes = nextRecipes.ToList();
            indexDirty = true;
        }

        public void EnsureIndex()
        {
            if (!indexDirty)
            {
                return;
            }

            itemsById.Clear();
            recipesByResultId.Clear();
            recipesByIngredientId.Clear();
            cachedItems.Clear();

            foreach (BlueprintItem item in items)
            {
                if (item == null || string.IsNullOrWhiteSpace(item.Id))
                {
                    continue;
                }

                itemsById[item.Id] = item;
                cachedItems.Add(item);
            }

            cachedItems.Sort((left, right) => string.Compare(left.DisplayName, right.DisplayName, StringComparison.Ordinal));

            foreach (BlueprintRecipe recipe in recipes)
            {
                if (recipe == null || string.IsNullOrWhiteSpace(recipe.ResultItemId))
                {
                    continue;
                }

                AddToIndex(recipesByResultId, recipe.ResultItemId, recipe);

                foreach (BlueprintIngredient ingredient in recipe.Ingredients)
                {
                    if (ingredient == null || string.IsNullOrWhiteSpace(ingredient.ItemId))
                    {
                        continue;
                    }

                    AddToIndex(recipesByIngredientId, ingredient.ItemId, recipe);
                }
            }

            indexDirty = false;
        }

        public BlueprintItem GetItem(string itemId)
        {
            EnsureIndex();
            itemsById.TryGetValue(itemId, out BlueprintItem item);
            return item;
        }

        public IReadOnlyList<BlueprintRecipe> GetRecipesForResult(string itemId)
        {
            EnsureIndex();
            return recipesByResultId.TryGetValue(itemId, out List<BlueprintRecipe> found) ? found : Array.Empty<BlueprintRecipe>();
        }

        public IReadOnlyList<BlueprintRecipe> GetRecipesUsingIngredient(string itemId)
        {
            EnsureIndex();
            return recipesByIngredientId.TryGetValue(itemId, out List<BlueprintRecipe> found) ? found : Array.Empty<BlueprintRecipe>();
        }

        public List<BlueprintItem> SearchItems(string query, List<BlueprintItem> results)
        {
            EnsureIndex();
            results.Clear();

            string normalizedQuery = Normalize(query);
            foreach (BlueprintItem item in cachedItems)
            {
                if (string.IsNullOrEmpty(normalizedQuery) || Normalize(item.DisplayName).Contains(normalizedQuery))
                {
                    results.Add(item);
                }
            }

            return results;
        }

        private static void AddToIndex(Dictionary<string, List<BlueprintRecipe>> index, string itemId, BlueprintRecipe recipe)
        {
            if (!index.TryGetValue(itemId, out List<BlueprintRecipe> indexedRecipes))
            {
                indexedRecipes = new List<BlueprintRecipe>();
                index.Add(itemId, indexedRecipes);
            }

            indexedRecipes.Add(recipe);
        }

        private static string Normalize(string text)
        {
            return string.IsNullOrWhiteSpace(text) ? string.Empty : text.Trim().ToLowerInvariant();
        }

        private void OnValidate()
        {
            indexDirty = true;
        }
    }
}
