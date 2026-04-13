using System.Collections.Generic;
using UnityEngine;

// 씬 안의 사람 목록을 관리하는 스크립트입니다.
// 지금은 등록/해제만 하지만, 나중에 "전체 사람 찾기" 기능의 중심이 될 수 있습니다.
public class PersonManager : MonoBehaviour
{
    // readonly는 persons 변수 자체를 다른 List로 바꾸지 않겠다는 뜻입니다.
    // List 안의 내용 추가/삭제는 가능합니다.
    private readonly List<PersonComponent> persons = new();

    // 외부에서는 목록을 읽기만 하게 IReadOnlyList로 공개합니다.
    public IReadOnlyList<PersonComponent> Persons => persons;

    // 사람을 목록에 등록합니다.
    public void Register(PersonComponent person)
    {
        // null이거나 이미 등록된 사람이라면 중복 등록하지 않습니다.
        if (person == null || persons.Contains(person))
        {
            return;
        }

        persons.Add(person);
    }

    // 사람을 목록에서 제거합니다.
    public void Unregister(PersonComponent person)
    {
        if (person == null)
        {
            return;
        }

        persons.Remove(person);
    }
}
