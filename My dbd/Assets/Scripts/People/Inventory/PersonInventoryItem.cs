using System;

// 인벤토리 안에 들어가는 아이템 한 종류를 표현합니다.
// 예: itemId = "wood1", count = 3 이면 wood1을 3개 가진 상태입니다.
[Serializable]
public class PersonInventoryItem
{
    // 아이템의 고유 이름/코드입니다. 지금은 wood1, stone1, tool1 같은 문자열을 씁니다.
    public string itemId;

    // 이 아이템을 몇 개 가지고 있는지 나타냅니다.
    public int count;

    // Unity 직렬화를 위해 비워 둔 생성자입니다.
    public PersonInventoryItem()
    {
    }

    // 코드에서 새 아이템 묶음을 만들 때 쓰는 생성자입니다.
    public PersonInventoryItem(string itemId, int count)
    {
        this.itemId = itemId;
        this.count = count;
    }
}
