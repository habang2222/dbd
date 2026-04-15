using System;
using UnityEngine;

namespace Dbd.Crafting
{
    public enum BlueprintItemCategory
    {
        Material,
        Intermediate,
        Weapon,
        Tool,
        Building,
        Placeable
    }

    [Serializable]
    public sealed class BlueprintItem
    {
        [SerializeField] private string id;
        [SerializeField] private string displayName;
        [SerializeField] private BlueprintItemCategory category;

        public string Id => id;
        public string DisplayName => displayName;
        public BlueprintItemCategory Category => category;

        public BlueprintItem(string id, string displayName, BlueprintItemCategory category)
        {
            this.id = id;
            this.displayName = displayName;
            this.category = category;
        }
    }
}
