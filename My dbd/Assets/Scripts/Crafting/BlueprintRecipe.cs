using System;
using System.Collections.Generic;
using UnityEngine;

namespace Dbd.Crafting
{
    [Serializable]
    public sealed class BlueprintRecipe
    {
        [SerializeField] private string id;
        [SerializeField] private string resultItemId;
        [SerializeField] private int resultAmount = 1;
        [SerializeField] private List<BlueprintIngredient> ingredients = new List<BlueprintIngredient>();

        public string Id => id;
        public string ResultItemId => resultItemId;
        public int ResultAmount => resultAmount;
        public IReadOnlyList<BlueprintIngredient> Ingredients => ingredients;

        public BlueprintRecipe(string id, string resultItemId, int resultAmount, params BlueprintIngredient[] ingredients)
        {
            this.id = id;
            this.resultItemId = resultItemId;
            this.resultAmount = resultAmount;
            this.ingredients = new List<BlueprintIngredient>(ingredients);
        }
    }
}
