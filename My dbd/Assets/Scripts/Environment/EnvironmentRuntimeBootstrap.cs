using Unity.AI.Navigation;
using UnityEngine;
using UnityEngine.AI;

// 게임 실행 시 바닥, 장애물, NavMesh를 자동으로 준비하는 클래스입니다.
// NavMesh는 사람이 길을 찾아 이동할 수 있는 영역입니다.
public static class EnvironmentRuntimeBootstrap
{
    private static readonly Vector3 GroundCenter = new(0f, -0.05f, 4f);
    private static readonly Vector3 GroundScale = new(90f, 0.1f, 60f);

    // 씬이 로드된 뒤 Unity가 자동으로 호출합니다.
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateEnvironmentIfMissing()
    {
        EnsureEnvironment();
    }

    // 다른 스크립트에서도 환경 준비를 요청할 수 있게 public으로 열어 둔 함수입니다.
    public static void EnsureEnvironment()
    {
        CreateGroundIfMissing();
        CreateObstacleIfMissing();
        RebuildNavMesh();
    }

    // Ground 오브젝트가 없으면 만들고, 있으면 필요한 컴포넌트를 보강합니다.
    private static void CreateGroundIfMissing()
    {
        GameObject ground = GameObject.Find("Ground");
        if (ground != null)
        {
            ground.transform.position = GroundCenter;
            ground.transform.localScale = GroundScale;
            EnsureSolidCollider(ground);

            // NavMeshSurface는 "이 오브젝트 주변을 기준으로 NavMesh를 만들겠다"는 컴포넌트입니다.
            if (ground.GetComponent<NavMeshSurface>() == null)
            {
                ConfigureSurface(ground.AddComponent<NavMeshSurface>(), ground);
            }
            else
            {
                ConfigureSurface(ground.GetComponent<NavMeshSurface>(), ground);
            }
            return;
        }

        ground = GameObject.CreatePrimitive(PrimitiveType.Cube);
        ground.name = "Ground";
        ground.transform.position = GroundCenter;
        ground.transform.localScale = GroundScale;
        EnsureSolidCollider(ground);
        ConfigureSurface(ground.AddComponent<NavMeshSurface>(), ground);

        Renderer renderer = ground.GetComponent<Renderer>();
        if (renderer != null)
        {
            // 바닥을 초록색 계열로 칠해서 눈에 잘 보이게 합니다.
            renderer.material.color = new Color(0.28f, 0.45f, 0.25f);
        }
    }

    // 테스트용 장애물 큐브를 만들거나, 기존 장애물에 필요한 설정을 붙입니다.
    private static void CreateObstacleIfMissing()
    {
        GameObject obstacle = GameObject.Find("Obstacle_1");
        if (obstacle != null)
        {
            EnsureObstacleComponents(obstacle);
            return;
        }

        obstacle = GameObject.CreatePrimitive(PrimitiveType.Cube);
        obstacle.name = "Obstacle_1";
        obstacle.transform.position = new Vector3(0f, 0.75f, 4f);
        obstacle.transform.localScale = new Vector3(1.5f, 1.5f, 1.5f);
        EnsureObstacleComponents(obstacle);

        Renderer renderer = obstacle.GetComponent<Renderer>();
        if (renderer != null)
        {
            // 장애물은 바닥과 구분되는 회색빛으로 칠합니다.
            renderer.material.color = new Color(0.45f, 0.42f, 0.38f);
        }
    }

    // Collider가 trigger면 물리적으로 막히지 않는 판정이 되므로, 일반 충돌체로 맞춥니다.
    private static void EnsureSolidCollider(GameObject target)
    {
        Collider collider = target.GetComponent<Collider>();
        if (collider != null)
        {
            collider.isTrigger = false;
        }
    }

    // 장애물이 NavMeshAgent의 길찾기에서 막는 물체로 취급되도록 설정합니다.
    private static void EnsureObstacleComponents(GameObject obstacle)
    {
        if (obstacle.GetComponent<ObstacleMarker>() == null)
        {
            obstacle.AddComponent<ObstacleMarker>();
        }

        EnsureSolidCollider(obstacle);

        NavMeshObstacle navObstacle = obstacle.GetComponent<NavMeshObstacle>();
        if (navObstacle == null)
        {
            navObstacle = obstacle.AddComponent<NavMeshObstacle>();
        }

        // carving은 장애물이 있는 부분을 NavMesh에서 파내듯이 막아 주는 옵션입니다.
        navObstacle.carving = true;
        navObstacle.carveOnlyStationary = false;
        navObstacle.shape = NavMeshObstacleShape.Box;
        navObstacle.center = Vector3.zero;
        navObstacle.size = obstacle.transform.localScale + new Vector3(0.4f, 0f, 0.4f);

        NavMeshModifier modifier = obstacle.GetComponent<NavMeshModifier>();
        if (modifier == null)
        {
            modifier = obstacle.AddComponent<NavMeshModifier>();
        }

        // area = 1은 보통 "Not Walkable" 영역입니다.
        // 즉 장애물 영역은 사람이 걷지 못하게 만듭니다.
        modifier.overrideArea = true;
        modifier.area = 1;
    }

    // 현재 NavMeshSurface를 찾아 즉시 NavMesh를 다시 만듭니다.
    private static void RebuildNavMesh()
    {
        NavMeshSurface surface = Object.FindFirstObjectByType<NavMeshSurface>();
        if (surface != null)
        {
            surface.BuildNavMesh();
        }
    }

    // NavMeshSurface가 바닥 크기에 맞는 영역을 수집하도록 설정합니다.
    private static void ConfigureSurface(NavMeshSurface surface, GameObject ground)
    {
        surface.collectObjects = CollectObjects.Volume;
        surface.center = Vector3.zero;

        // y축으로 2를 더해서 바닥 바로 위의 사람/장애물까지 계산 범위에 들어오게 합니다.
        surface.size = ground.transform.localScale + new Vector3(0f, 2f, 0f);
    }
}
