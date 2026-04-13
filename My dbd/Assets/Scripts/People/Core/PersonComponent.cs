using UnityEngine;

// 사람 한 명의 "기본 정보"를 들고 있는 컴포넌트입니다.
// 큐브 오브젝트에 이 스크립트를 붙이면 그 큐브가 게임 안에서 사람처럼 취급됩니다.
public class PersonComponent : MonoBehaviour
{
    // Inspector에서 접기 쉬우라고 영역 이름을 붙여 둔 것입니다.
    [Header("Identity")]
    // 내부 식별자입니다. 예: person_1
    [SerializeField] private string personId;

    // 화면/로그에 보여줄 이름입니다. 예: Person_1
    [SerializeField] private string personName;

    [Header("Core Data")]
    // 체력, 힘, 스태미나 같은 능력치 묶음입니다.
    [SerializeField] private PersonStats stats = new();

    // 사람이 들고 있는 아이템 목록입니다.
    [SerializeField] private PersonInventory inventory = new();

    [Header("Future Extension Points")]
    // 나중에 AI 상태를 넣기 위한 자리입니다. 지금은 기본값으로 Idle을 사용합니다.
    [SerializeField] private string currentState = "Idle";

    // 나중에 현재 행동을 넣기 위한 자리입니다.
    [SerializeField] private string currentAction = "None";

    // 나중에 팀/진영 기능을 넣기 위한 자리입니다.
    [SerializeField] private string teamId;

    // 지금 이 사람이 선택되었는지 저장합니다.
    [SerializeField] private bool isSelected;

    // 아래 public 속성들은 다른 스크립트가 값을 읽을 수 있게 해 줍니다.
    // set이 없으므로 외부에서 함부로 바꾸지는 못합니다.
    public string PersonId => personId;
    public string PersonName => personName;
    public PersonStats Stats => stats;
    public PersonInventory Inventory => inventory;
    public string CurrentState => currentState;
    public string CurrentAction => currentAction;
    public string TeamId => teamId;
    public bool IsSelected => isSelected;

    // Collider가 붙은 오브젝트를 마우스로 클릭하면 Unity가 자동으로 호출합니다.
    private void OnMouseDown()
    {
        Select();
    }

    // 새 사람을 만들 때 기본 정보를 한 번에 넣는 함수입니다.
    public void Initialize(string id, string displayName, PersonStats initialStats, PersonInventory initialInventory)
    {
        personId = id;
        personName = displayName;

        // ??는 왼쪽 값이 null이면 오른쪽 값을 대신 쓰라는 뜻입니다.
        stats = initialStats ?? new PersonStats();
        inventory = initialInventory ?? new PersonInventory();
        currentState = "Idle";
        currentAction = "None";
        teamId = string.Empty;
        isSelected = false;

        // Unity Hierarchy 창에서도 사람 이름이 보이게 오브젝트 이름을 맞춥니다.
        gameObject.name = personName;
    }

    // 선택 상태만 바꾸는 작은 함수입니다.
    public void SetSelected(bool selected)
    {
        isSelected = selected;
    }

    public void SetUnitStatus(string state, string action)
    {
        if (!string.IsNullOrWhiteSpace(state))
        {
            currentState = state;
        }

        if (!string.IsNullOrWhiteSpace(action))
        {
            currentAction = action;
        }

        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }
    }

    // 이 사람을 선택합니다.
    public void Select()
    {
        // 한 번에 한 명만 선택되게, 먼저 모든 사람의 선택을 해제합니다.
        foreach (PersonComponent person in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            person.SetSelected(false);
        }

        SetSelected(true);

        // 제작 테스트 UI가 있으면 선택된 사람 정보를 패널에 보여 줍니다.
        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }

        Debug.Log($"Selected {personName}: health={stats.health}, strength={stats.strength}, stamina={stats.stamina}");
    }

    // 나중에 팀 시스템이 필요할 때 쓸 수 있게 열어 둔 함수입니다.
    public void SetTeam(string newTeamId)
    {
        teamId = newTeamId;
    }
}
