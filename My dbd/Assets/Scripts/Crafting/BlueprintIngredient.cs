using System;
using UnityEngine;

namespace Dbd.Crafting
{
    [Serializable]
    public sealed class BlueprintIngredient
    {
        [SerializeField] private string itemId;
        [SerializeField] private int amount;

        public string ItemId => itemId;
        public int Amount => amount;

        public BlueprintIngredient(string itemId, int amount)
        {
            this.itemId = itemId;
            this.amount = amount;
        }
    }
}
