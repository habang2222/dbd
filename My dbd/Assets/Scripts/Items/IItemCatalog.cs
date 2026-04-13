namespace Dbd.Items
{
    public interface IItemCatalog
    {
        bool TryGetItem(string itemId, out ItemData itemData);
        int GetMaxStack(string itemId);
    }
}
