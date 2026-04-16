using UnityEngine;

// 씬에 사람이 부족하면 새 사람 큐브를 만들어 주는 스크립트입니다.
// "Spawner"는 게임에서 오브젝트를 생성하는 역할을 부르는 이름입니다.
public class PersonSpawner : MonoBehaviour
{
    // 만들고 싶은 사람 수입니다.
    [SerializeField] private int personCount = 4;

    // 사람을 나란히 만들 때 서로 떨어질 간격입니다.
    [SerializeField] private float spacing = 2f;

    // 생성한 사람을 등록할 관리자입니다. 비어 있으면 Start에서 자동으로 찾습니다.
    [SerializeField] private PersonManager personManager;

    // Start는 이 컴포넌트가 활성화된 뒤 첫 프레임 직전에 한 번 호출됩니다.
    private void Start()
    {
        // 이미 충분한 사람이 있으면 추가 생성하지 않습니다.
        if (FindObjectsByType<PersonComponent>(FindObjectsSortMode.None).Length >= personCount)
        {
            return;
        }

        if (personManager == null)
        {
            personManager = FindFirstObjectByType<PersonManager>();
        }

        if (personManager == null)
        {
            GameObject managerObject = new GameObject("Person Manager");
            personManager = managerObject.AddComponent<PersonManager>();
        }

        // personCount만큼 반복해서 사람을 만듭니다.
        for (int i = 0; i < personCount; i++)
        {
            SpawnPerson(i);
        }
    }

    // index는 0부터 시작합니다. 표시 이름은 보기 좋게 1부터 시작하도록 index + 1을 씁니다.
    private void SpawnPerson(int index)
    {
        string displayName = $"Person_{index + 1}";
        Vector3 position = transform.position + new Vector3(index * spacing, 0f, 0f);
        position.y = EnvironmentRuntimeBootstrap.GetTerrainHeight(position) + 1.2f;

        // 지금은 임시 시각화용으로 Cube를 사람처럼 사용합니다.
        GameObject personObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
        personObject.transform.position = position;
        personObject.transform.localScale = new Vector3(1.25f, 1.25f, 1.25f);

        Renderer renderer = personObject.GetComponent<Renderer>();
        if (renderer != null)
        {
            // 사람마다 색을 조금씩 다르게 해서 구분하기 쉽게 합니다.
            renderer.material.color = Color.HSVToRGB(index / 4f, 0.75f, 0.95f);
        }

        // 사람 정보 컴포넌트를 붙이고 초기 데이터를 넣습니다.
        PersonComponent person = personObject.AddComponent<PersonComponent>();
        person.Initialize(
            $"person_{index + 1}",
            displayName,
            CreateStats(index),
            CreateInventory(index));

        // 이동 컴포넌트도 같이 붙여 둡니다.
        // 처음에는 멈춰 있고, 우클릭 목적지를 받으면 움직입니다.
        PersonMover mover = personObject.AddComponent<PersonMover>();
        mover.InitializeIdle(position, 1.5f + (index * 0.25f));
        personObject.AddComponent<UnitCombatController>();
        personObject.AddComponent<UnitDeathShrink>();

        personManager.Register(person);
    }

    // 사람별로 약간 다른 능력치를 만들어 줍니다.
    private PersonStats CreateStats(int index)
    {
        return new PersonStats(
            health: 100f,
            strength: 10f + index,
            stamina: 100f);
    }

    // 테스트용 기본 아이템을 만들어 줍니다.
    private PersonInventory CreateInventory(int index)
    {
        PersonInventory inventory = new PersonInventory();
        inventory.AddItem("wood_1", index + 1);
        inventory.AddItem("stone_1", 1);
        return inventory;
    }
}
