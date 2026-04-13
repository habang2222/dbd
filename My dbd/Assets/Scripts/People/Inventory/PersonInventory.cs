using System;
using System.Collections.Generic;
using System.Linq;

// 사람 한 명의 인벤토리입니다.
// List는 "여러 개를 담는 상자"라고 보면 됩니다.
[Serializable]
public class PersonInventory
{
    // 사람이 가진 아이템 목록입니다.
    // public으로 둔 이유는 Unity Inspector/테스트 UI에서 쉽게 확인하기 위해서입니다.
    public List<PersonInventoryItem> items = new();

    // 특정 아이템을 총 몇 개 가지고 있는지 계산합니다.
    public int GetItemCount(string itemId)
    {
        // 비어 있는 문자열이나 null은 잘못된 아이템 이름으로 보고 0을 돌려줍니다.
        if (string.IsNullOrWhiteSpace(itemId))
        {
            return 0;
        }

        // Where로 같은 itemId만 고르고, Sum으로 count를 모두 더합니다.
        return items
            .Where(item => item != null && item.itemId == itemId)
            .Sum(item => item.count);
    }

    // 아이템을 추가합니다.
    public void AddItem(string itemId, int count)
    {
        // 아이템 이름이 없거나 개수가 0 이하이면 아무것도 하지 않습니다.
        if (string.IsNullOrWhiteSpace(itemId) || count <= 0)
        {
            return;
        }

        // 이미 같은 itemId가 있으면 그 칸의 개수만 늘립니다.
        PersonInventoryItem item = items.FirstOrDefault(entry => entry != null && entry.itemId == itemId);
        if (item == null)
        {
            // 같은 itemId가 없으면 새 칸을 만들어 목록에 추가합니다.
            items.Add(new PersonInventoryItem(itemId, count));
            return;
        }

        item.count += count;
    }

    // 아이템을 count개만큼 뺄 수 있는지 미리 확인합니다.
    public bool CanRemoveItem(string itemId, int count)
    {
        if (string.IsNullOrWhiteSpace(itemId) || count <= 0)
        {
            return false;
        }

        return GetItemCount(itemId) >= count;
    }

    // 아이템을 실제로 제거합니다. 성공하면 true, 실패하면 false입니다.
    public bool RemoveItem(string itemId, int count)
    {
        if (!CanRemoveItem(itemId, count))
        {
            return false;
        }

        // remaining은 앞으로 더 빼야 하는 개수입니다.
        int remaining = count;

        // 뒤에서부터 도는 이유:
        // List에서 RemoveAt으로 삭제할 때 앞에서부터 지우면 인덱스가 밀려 헷갈릴 수 있습니다.
        for (int i = items.Count - 1; i >= 0 && remaining > 0; i--)
        {
            PersonInventoryItem item = items[i];
            if (item == null || item.itemId != itemId)
            {
                continue;
            }

            // Math.Min은 둘 중 더 작은 값을 고릅니다.
            // 예: 아이템 칸에는 1개만 있는데 3개를 빼야 하면 일단 1개만 뺍니다.
            int amount = Math.Min(item.count, remaining);
            item.count -= amount;
            remaining -= amount;

            // 개수가 0 이하가 된 아이템 칸은 목록에서 제거합니다.
            if (item.count <= 0)
            {
                items.RemoveAt(i);
            }
        }

        return true;
    }
}
