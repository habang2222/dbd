using System.Collections.Generic;
using UnityEngine;

namespace Dbd.Items
{
    [CreateAssetMenu(menuName = "DBD/Items/Item Data")]
    public class ItemData : ScriptableObject
    {
        [SerializeField] private string itemId;
        [SerializeField] private string displayName;
        [SerializeField] private int maxStack = 99;
        [SerializeField] private Sprite icon;
        [SerializeField] private List<string> tags = new();

        public string ItemId => itemId;
        public string DisplayName => displayName;
        public int MaxStack => Mathf.Max(1, maxStack);
        public Sprite Icon => icon;
        public IReadOnlyList<string> Tags => tags;
    }
}
