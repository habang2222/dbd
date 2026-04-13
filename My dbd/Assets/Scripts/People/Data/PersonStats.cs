using System;

// 사람의 능력치를 담는 단순한 데이터 클래스입니다.
// [Serializable]이 있으면 Unity Inspector나 씬 파일에 이 값들을 저장하기 쉬워집니다.
[Serializable]
public class PersonStats
{
    // 체력입니다. 나중에 피해/회복 시스템을 만들 때 사용할 수 있습니다.
    public float health = 100f;

    // 힘입니다. 지금은 표시용이지만, 나중에 작업 속도나 전투력에 연결할 수 있습니다.
    public float strength = 10f;

    // 스태미나입니다. 나중에 달리기/작업 피로도에 연결할 수 있습니다.
    public float stamina = 100f;

    // Unity 직렬화는 빈 생성자를 좋아합니다.
    // 그래서 아래처럼 아무것도 안 받는 생성자를 남겨 둡니다.
    public PersonStats()
    {
    }

    // 코드에서 능력치를 한 번에 만들고 싶을 때 쓰는 생성자입니다.
    public PersonStats(float health, float strength, float stamina)
    {
        this.health = health;
        this.strength = strength;
        this.stamina = stamina;
    }
}
