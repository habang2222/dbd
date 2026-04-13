using Unity.AI.Navigation;
using UnityEngine;

// 게임 실행 시 사람 관련 기본 세팅을 자동으로 준비하는 클래스입니다.
// static class라서 오브젝트에 붙이지 않아도 Unity가 RuntimeInitializeOnLoadMethod를 보고 실행합니다.
public static class PersonRuntimeBootstrap
{
    // 씬 로드가 끝난 뒤 자동으로 호출됩니다.
    // 직접 버튼을 누르지 않아도 테스트용 사람이 준비되게 하려는 코드입니다.
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreatePeopleIfMissing()
    {
        // 사람이 걸어 다닐 환경과 NavMesh가 먼저 필요합니다.
        EnvironmentRuntimeBootstrap.EnsureEnvironment();

        // 이미 씬에 사람이 있다면 새로 만들지 않고, 빠진 기능만 보강합니다.
        if (Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None).Length > 0)
        {
            EnsureExistingPeopleCanMove();
            EnvironmentRuntimeBootstrap.EnsureEnvironment();
            PositionCamera();
            EnsureClickMoveController();
            return;
        }

        // 사람 목록을 관리할 PersonManager를 찾거나 새로 만듭니다.
        PersonManager manager = Object.FindFirstObjectByType<PersonManager>();
        if (manager == null)
        {
            GameObject managerObject = new GameObject("Person Manager");
            manager = managerObject.AddComponent<PersonManager>();
        }

        // 테스트용 사람 4명을 생성합니다.
        for (int i = 0; i < 4; i++)
        {
            string personName = $"Person_{i + 1}";

            // 지금은 모델이 없으므로 Cube를 사람 대용으로 씁니다.
            GameObject personObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
            personObject.name = personName;
            personObject.transform.position = new Vector3(-3f + (i * 2f), 1f, 4f);
            personObject.transform.localScale = new Vector3(1.5f, 1.5f, 1.5f);
            EnsurePersonCollision(personObject);

            Renderer renderer = personObject.GetComponent<Renderer>();
            if (renderer != null)
            {
                // HSV 색상으로 사람마다 다른 색을 줍니다.
                renderer.material.color = Color.HSVToRGB(i / 4f, 0.75f, 0.95f);
            }

            // 제작 테스트를 위해 기본 재료를 넣습니다.
            PersonInventory inventory = new PersonInventory();
            inventory.AddItem("wood1", i + 1);
            inventory.AddItem("stone1", 1);

            // 사람의 기본 데이터 컴포넌트를 붙입니다.
            PersonComponent person = personObject.AddComponent<PersonComponent>();
            person.Initialize(
                $"person_{i + 1}",
                personName,
                new PersonStats(100f, 10f + i, 100f),
                inventory);

            // 이동 기능을 붙입니다. 처음에는 멈춘 상태입니다.
            PersonMover mover = personObject.AddComponent<PersonMover>();
            mover.InitializeIdle(personObject.transform.position, 1.5f + (i * 0.25f));

            manager.Register(person);
        }

        PositionCamera();
        EnsureClickMoveController();
        Debug.Log("Created runtime Person_1 ~ Person_4 cubes.");
    }

    // 테스트하기 좋게 카메라 위치와 카메라 이동 기능을 맞춥니다.
    private static void PositionCamera()
    {
        Camera camera = Camera.main;
        if (camera == null)
        {
            camera = Object.FindFirstObjectByType<Camera>();
        }

        if (camera == null)
        {
            GameObject cameraObject = new GameObject("Main Camera");
            cameraObject.tag = "MainCamera";
            camera = cameraObject.AddComponent<Camera>();
        }

        // 위에서 비스듬히 내려다보는 위치입니다.
        camera.transform.position = new Vector3(0f, 10f, -8f);
        camera.transform.rotation = Quaternion.Euler(55f, 0f, 0f);
        camera.fieldOfView = 50f;

        // WASD/휠 카메라 조작 스크립트가 없으면 붙여 줍니다.
        if (camera.GetComponent<CameraPanZoomController>() == null)
        {
            camera.gameObject.AddComponent<CameraPanZoomController>();
        }
    }

    // 우클릭 목적지 지정 컨트롤러가 씬에 하나는 있도록 보장합니다.
    private static void EnsureClickMoveController()
    {
        if (Object.FindFirstObjectByType<PersonClickMoveController>() != null)
        {
            return;
        }

        GameObject controllerObject = new GameObject("Person Click Move Controller");
        controllerObject.AddComponent<PersonClickMoveController>();
    }

    // 이미 씬에 놓여 있는 사람들에게 이동 기능과 충돌 설정을 보강합니다.
    private static void EnsureExistingPeopleCanMove()
    {
        PersonComponent[] people = Object.FindObjectsByType<PersonComponent>(FindObjectsSortMode.None);
        for (int i = 0; i < people.Length; i++)
        {
            PersonComponent person = people[i];
            EnsurePersonCollision(person.gameObject);
            if (person.GetComponent<PersonMover>() != null)
            {
                continue;
            }

            PersonMover mover = person.gameObject.AddComponent<PersonMover>();
            mover.InitializeIdle(person.transform.position, 1.5f + (i * 0.25f));
        }
    }

    // 사람 오브젝트가 NavMesh 빌드나 클릭 판정에서 이상하게 동작하지 않게 기본 설정을 맞춥니다.
    private static void EnsurePersonCollision(GameObject personObject)
    {
        Collider collider = personObject.GetComponent<Collider>();
        if (collider != null)
        {
            collider.isTrigger = false;
        }

        NavMeshModifier modifier = personObject.GetComponent<NavMeshModifier>();
        if (modifier == null)
        {
            modifier = personObject.AddComponent<NavMeshModifier>();
        }

        // 사람 큐브 자체는 NavMesh 바닥 계산에 포함하지 않습니다.
        modifier.ignoreFromBuild = true;

        Rigidbody body = personObject.GetComponent<Rigidbody>();
        if (body != null)
        {
            // 현재 이동은 NavMeshAgent가 담당하므로 Rigidbody 물리는 제거합니다.
            Object.Destroy(body);
        }
    }
}
