using System;

namespace Dbd.Items
{
    [Serializable]
    public struct ItemStack
    {
        public string ItemId;
        public int Count;

        public ItemStack(string itemId, int count)
        {
            ItemId = itemId;
            Count = count;
        }

        public bool IsValid => !string.IsNullOrWhiteSpace(ItemId) && Count > 0;
    }
}
