using System;

namespace Dbd.Items
{
    [Serializable]
    public class InventorySlot
    {
        public string ItemId;
        public int Count;

        public bool IsEmpty => string.IsNullOrWhiteSpace(ItemId) || Count <= 0;

        public void Clear()
        {
            ItemId = string.Empty;
            Count = 0;
        }
    }
}
