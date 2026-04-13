using System;
using System.Collections.Generic;
using System.Linq;

namespace Dbd.Items
{
    [Serializable]
    public class Inventory
    {
        private readonly List<InventorySlot> slots = new();
        private readonly IItemCatalog itemCatalog;

        public Inventory(int slotCount, IItemCatalog itemCatalog)
        {
            if (slotCount < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(slotCount));
            }

            this.itemCatalog = itemCatalog ?? DefaultItemCatalog.Instance;
            for (int i = 0; i < slotCount; i++)
            {
                slots.Add(new InventorySlot());
            }
        }

        public IReadOnlyList<InventorySlot> Slots => slots;

        public int GetItemCount(string itemId)
        {
            if (string.IsNullOrWhiteSpace(itemId))
            {
                return 0;
            }

            return slots.Where(slot => slot.ItemId == itemId).Sum(slot => slot.Count);
        }

        public bool CanAddItems(IEnumerable<ItemStack> itemStacks)
        {
            List<ItemStack> stacks = Normalize(itemStacks);
            List<InventorySlot> simulatedSlots = CloneSlots();

            foreach (ItemStack stack in stacks)
            {
                if (!TryAddToSlots(simulatedSlots, stack))
                {
                    return false;
                }
            }

            return true;
        }

        public bool AddItems(IEnumerable<ItemStack> itemStacks)
        {
            List<ItemStack> stacks = Normalize(itemStacks);
            if (!CanAddItems(stacks))
            {
                return false;
            }

            foreach (ItemStack stack in stacks)
            {
                TryAddToSlots(slots, stack);
            }

            return true;
        }

        public bool CanRemoveItems(IEnumerable<ItemStack> itemStacks)
        {
            foreach (ItemStack stack in Normalize(itemStacks))
            {
                if (GetItemCount(stack.ItemId) < stack.Count)
                {
                    return false;
                }
            }

            return true;
        }

        public bool RemoveItems(IEnumerable<ItemStack> itemStacks)
        {
            List<ItemStack> stacks = Normalize(itemStacks);
            if (!CanRemoveItems(stacks))
            {
                return false;
            }

            foreach (ItemStack stack in stacks)
            {
                int remaining = stack.Count;
                for (int i = slots.Count - 1; i >= 0 && remaining > 0; i--)
                {
                    InventorySlot slot = slots[i];
                    if (slot.ItemId != stack.ItemId)
                    {
                        continue;
                    }

                    int amount = Math.Min(slot.Count, remaining);
                    slot.Count -= amount;
                    remaining -= amount;

                    if (slot.Count <= 0)
                    {
                        slot.Clear();
                    }
                }
            }

            return true;
        }

        public bool CanExchangeItems(IEnumerable<ItemStack> inputs, IEnumerable<ItemStack> outputs)
        {
            List<ItemStack> normalizedInputs = Normalize(inputs);
            List<ItemStack> normalizedOutputs = Normalize(outputs);

            if (!CanRemoveItems(normalizedInputs))
            {
                return false;
            }

            List<InventorySlot> simulatedSlots = CloneSlots();
            foreach (ItemStack input in normalizedInputs)
            {
                RemoveFromSlots(simulatedSlots, input);
            }

            foreach (ItemStack output in normalizedOutputs)
            {
                if (!TryAddToSlots(simulatedSlots, output))
                {
                    return false;
                }
            }

            return true;
        }

        private bool TryAddToSlots(List<InventorySlot> targetSlots, ItemStack stack)
        {
            int remaining = stack.Count;
            int maxStack = itemCatalog.GetMaxStack(stack.ItemId);

            foreach (InventorySlot slot in targetSlots)
            {
                if (slot.ItemId != stack.ItemId)
                {
                    continue;
                }

                int space = maxStack - slot.Count;
                if (space <= 0)
                {
                    continue;
                }

                int amount = Math.Min(space, remaining);
                slot.Count += amount;
                remaining -= amount;

                if (remaining <= 0)
                {
                    return true;
                }
            }

            foreach (InventorySlot slot in targetSlots)
            {
                if (!slot.IsEmpty)
                {
                    continue;
                }

                int amount = Math.Min(maxStack, remaining);
                slot.ItemId = stack.ItemId;
                slot.Count = amount;
                remaining -= amount;

                if (remaining <= 0)
                {
                    return true;
                }
            }

            return false;
        }

        private void RemoveFromSlots(List<InventorySlot> targetSlots, ItemStack stack)
        {
            int remaining = stack.Count;
            for (int i = targetSlots.Count - 1; i >= 0 && remaining > 0; i--)
            {
                InventorySlot slot = targetSlots[i];
                if (slot.ItemId != stack.ItemId)
                {
                    continue;
                }

                int amount = Math.Min(slot.Count, remaining);
                slot.Count -= amount;
                remaining -= amount;

                if (slot.Count <= 0)
                {
                    slot.Clear();
                }
            }
        }

        private List<InventorySlot> CloneSlots()
        {
            return slots.Select(slot => new InventorySlot
            {
                ItemId = slot.ItemId,
                Count = slot.Count
            }).ToList();
        }

        private static List<ItemStack> Normalize(IEnumerable<ItemStack> itemStacks)
        {
            return itemStacks?
                .Where(stack => stack.IsValid)
                .GroupBy(stack => stack.ItemId)
                .Select(group => new ItemStack(group.Key, group.Sum(stack => stack.Count)))
                .ToList() ?? new List<ItemStack>();
        }

        private class DefaultItemCatalog : IItemCatalog
        {
            public static readonly DefaultItemCatalog Instance = new();

            public bool TryGetItem(string itemId, out ItemData itemData)
            {
                itemData = null;
                return false;
            }

            public int GetMaxStack(string itemId)
            {
                return 99;
            }
        }
    }
}
