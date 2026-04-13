using System.Collections.Generic;
using UnityEngine;

namespace Dbd.Items
{
    [CreateAssetMenu(menuName = "DBD/Items/Item Catalog")]
    public class ItemCatalog : ScriptableObject, IItemCatalog
    {
        [SerializeField] private List<ItemData> items = new();

        private Dictionary<string, ItemData> itemById;

        public bool TryGetItem(string itemId, out ItemData itemData)
        {
            EnsureCache();
            return itemById.TryGetValue(itemId, out itemData);
        }

        public int GetMaxStack(string itemId)
        {
            return TryGetItem(itemId, out ItemData itemData) ? itemData.MaxStack : 1;
        }

        private void EnsureCache()
        {
            if (itemById != null)
            {
                return;
            }

            itemById = new Dictionary<string, ItemData>();
            foreach (ItemData item in items)
            {
                if (item == null || string.IsNullOrWhiteSpace(item.ItemId))
                {
                    continue;
                }

                itemById[item.ItemId] = item;
            }
        }

        private void OnValidate()
        {
            itemById = null;
        }
    }
}
