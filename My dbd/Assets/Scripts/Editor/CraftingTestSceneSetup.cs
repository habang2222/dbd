using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

// Unity Editor 메뉴에서 테스트 씬을 빠르게 구성하기 위한 도구입니다.
// 이 파일은 Assets/Scripts/Editor 안에 있으므로, 빌드된 게임이 아니라 Unity Editor에서만 사용됩니다.
public static class CraftingTestSceneSetup
{
    // SampleScene에 제작 패널, 사람 관리자, 사람 생성기, 카메라 위치를 세팅합니다.
    public static void Install()
    {
        const string scenePath = "Assets/Scenes/SampleScene.unity";

        // 지정한 씬을 에디터에서 엽니다.
        Scene scene = EditorSceneManager.OpenScene(scenePath);

        // 제작 테스트 패널이 없으면 씬에 추가합니다.
        // 사람 목록 관리자가 없으면 추가합니다.
        if (Object.FindFirstObjectByType<PersonManager>() == null)
        {
            GameObject managerObject = new GameObject("Person Manager");
            managerObject.AddComponent<PersonManager>();
            EditorSceneManager.MarkSceneDirty(scene);
        }

        // 사람 생성기가 없으면 추가합니다.
        if (Object.FindFirstObjectByType<PersonSpawner>() == null)
        {
            GameObject spawnerObject = new GameObject("Person Spawner");
            spawnerObject.transform.position = new Vector3(-3f, 0.5f, 0f);
            spawnerObject.AddComponent<PersonSpawner>();
            EditorSceneManager.MarkSceneDirty(scene);
        }

        CreateVisiblePersonCubes();
        PositionCameraForPersonTest();
        EditorSceneManager.SaveScene(scene);
    }

    // 눈에 보이는 테스트용 사람 큐브 4개를 만들거나, 이미 있으면 위치/데이터를 갱신합니다.
    private static void CreateVisiblePersonCubes()
    {
        PersonManager manager = Object.FindFirstObjectByType<PersonManager>();

        for (int i = 0; i < 4; i++)
        {
            string personName = $"Person_{i + 1}";
            GameObject existing = GameObject.Find(personName);
            GameObject personObject = existing;
            if (personObject == null)
            {
                // 사람 모델이 없으므로 Cube를 임시 사람으로 만듭니다.
                personObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
                personObject.name = personName;
            }

            personObject.transform.position = new Vector3(-3f + (i * 2f), 1f, 4f);
            personObject.transform.localScale = new Vector3(1.5f, 1.5f, 1.5f);

            Renderer renderer = personObject.GetComponent<Renderer>();
            if (renderer != null)
            {
                // sharedMaterial은 에디터에서 씬에 저장되는 재질을 다룰 때 사용합니다.
                if (renderer.sharedMaterial == null || renderer.sharedMaterial.name == "Default-Material")
                {
                    Shader shader = Shader.Find("Universal Render Pipeline/Lit");
                    if (shader == null)
                    {
                        shader = Shader.Find("Standard");
                    }

                    if (shader != null)
                    {
                        renderer.sharedMaterial = new Material(shader);
                    }
                }

                if (renderer.sharedMaterial != null)
                {
                    renderer.sharedMaterial.color = Color.HSVToRGB(i / 4f, 0.75f, 0.95f);
                }
            }

            // PersonComponent가 없으면 붙이고, 있으면 데이터를 다시 초기화합니다.
            PersonComponent person = personObject.GetComponent<PersonComponent>();
            if (person == null)
            {
                person = personObject.AddComponent<PersonComponent>();
            }

            // 제작 테스트용 기본 재료입니다.
            PersonInventory inventory = new PersonInventory();
            inventory.AddItem("wood1", i + 1);
            inventory.AddItem("stone1", 1);
            person.Initialize(
                $"person_{i + 1}",
                personName,
                new PersonStats(100f, 10f + i, 100f),
                inventory);

            if (manager != null)
            {
                manager.Register(person);
            }

            EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
        }
    }

    // 테스트 씬을 잘 볼 수 있도록 카메라를 배치합니다.
    private static void PositionCameraForPersonTest()
    {
        Camera camera = Object.FindFirstObjectByType<Camera>();
        if (camera == null)
        {
            GameObject cameraObject = new GameObject("Main Camera");
            cameraObject.tag = "MainCamera";
            camera = cameraObject.AddComponent<Camera>();
        }

        camera.transform.position = new Vector3(0f, 10f, -8f);
        camera.transform.rotation = Quaternion.Euler(55f, 0f, 0f);
        camera.clearFlags = CameraClearFlags.Skybox;
        camera.orthographic = false;
        camera.fieldOfView = 50f;
        EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
    }

    // Unity editor menu entry for rebuilding the test scene.
    [MenuItem("DBD/Install Test Scene")]
    public static void InstallFromMenu()
    {
        Install();
    }
}
