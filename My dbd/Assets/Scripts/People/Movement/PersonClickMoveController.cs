using UnityEngine;
using UnityEngine.EventSystems;

// 선택된 사람에게 "어디로 갈지" 알려주는 입력 담당 스크립트입니다.
// 사용 방법:
// 1. PersonComponent가 붙은 사람 오브젝트를 좌클릭해서 선택합니다.
// 2. Ground 바닥을 우클릭하면, 선택된 사람이 그 위치로 이동합니다.
public class PersonClickMoveController : MonoBehaviour
{
    // Ray를 쏠 카메라입니다. 비어 있으면 Awake/Update에서 Camera.main을 자동으로 찾습니다.
    [SerializeField] private Camera targetCamera;

    // Raycast가 맞출 수 있는 레이어입니다. ~0은 "모든 레이어"라는 뜻입니다.
    [SerializeField] private LayerMask destinationLayers = ~0;

    // 카메라에서 마우스 방향으로 얼마나 멀리까지 클릭 검사를 할지 정합니다.
    [SerializeField] private float maxRayDistance = 500f;

    // Awake는 이 컴포넌트가 처음 준비될 때 Unity가 한 번 호출합니다.
    private void Awake()
    {
        if (targetCamera == null)
        {
            targetCamera = Camera.main;
        }
    }

    // Update는 게임이 실행되는 동안 매 프레임 호출됩니다.
    private void Update()
    {
        // GetMouseButtonDown(1)은 "우클릭을 막 누른 순간"만 true입니다.
        // UI 위에서 우클릭한 경우에는 게임 명령으로 처리하지 않습니다.
        if (!Input.GetMouseButtonDown(1) || IsPointerOverUi())
        {
            return;
        }

        // 카메라가 아직 연결되지 않았거나 씬에서 바뀌었을 수 있으니 한 번 더 찾습니다.
        if (targetCamera == null)
        {
            targetCamera = Camera.main;
        }

        if (targetCamera == null)
        {
            return;
        }

        // 마우스 화면 좌표를 3D 월드 방향의 Ray로 바꿉니다.
        // Ray는 "카메라에서 마우스가 가리키는 방향으로 나가는 보이지 않는 선"이라고 보면 됩니다.
        Ray ray = targetCamera.ScreenPointToRay(Input.mousePosition);

        // Raycast는 Ray가 실제 Collider와 부딪혔는지 검사합니다.
        // out RaycastHit hit은 "맞은 물체 정보"를 hit 변수에 담아 달라는 뜻입니다.
        if (!Physics.Raycast(ray, out RaycastHit hit, maxRayDistance, destinationLayers))
        {
            return;
        }

        // 사람을 우클릭한 경우에는 목적지로 쓰지 않습니다.
        // 목적지는 오직 Ground 바닥만 허용해서 장애물/사람을 잘못 찍는 일을 줄입니다.
        if (hit.collider.GetComponentInParent<PersonComponent>() != null || !IsDestinationSurface(hit.collider))
        {
            return;
        }

        // 현재 선택된 사람을 찾습니다. 선택된 사람이 없다면 이동할 대상도 없습니다.
        PersonComponent selectedPerson = FindSelectedPerson();
        if (selectedPerson == null)
        {
            Debug.Log("Select a person first, then click the ground to set their destination.");
            return;
        }

        // 선택된 사람에게 이동 기능(PersonMover)이 없으면 붙여 줍니다.
        // AddComponent는 Unity 오브젝트에 새 기능 부품을 끼우는 느낌입니다.
        PersonMover mover = selectedPerson.GetComponent<PersonMover>();
        if (mover == null)
        {
            mover = selectedPerson.gameObject.AddComponent<PersonMover>();
        }

        // 실제 이동 명령은 PersonMover가 담당합니다.
        mover.MoveToDestination(hit.point);
        selectedPerson.SetUnitStatus("\uC774\uB3D9 \uC911", "\uD2B9\uC815 \uC9C0\uC5ED \uC774\uB3D9");
        Debug.Log($"Moved {selectedPerson.PersonName} destination to {hit.point}.");
    }

    // 씬 안의 모든 PersonComponent를 돌면서 isSelected가 true인 사람을 찾습니다.
    private static PersonComponent FindSelectedPerson()
    {
        foreach (PersonComponent person in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (person.IsSelected)
            {
                return person;
            }
        }

        return null;
    }

    // UI 버튼이나 패널 위에서 클릭했는지 확인합니다.
    // EventSystem이 없으면 UI 검사를 할 수 없으므로 false로 처리합니다.
    private static bool IsPointerOverUi()
    {
        return EventSystem.current != null && EventSystem.current.IsPointerOverGameObject();
    }

    // 지금 프로젝트에서는 바닥 오브젝트 이름을 "Ground"로 쓰고 있습니다.
    // 이름 검사라 단순하지만, 초반 프로젝트에서는 찾기 쉽고 이해하기 쉬운 방식입니다.
    private static bool IsDestinationSurface(Collider collider)
    {
        return collider != null && collider.gameObject.name == "Ground";
    }
}
