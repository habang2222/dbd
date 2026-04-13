using UnityEngine;
using UnityEngine.AI;

// 사람 오브젝트를 실제로 움직이게 하는 스크립트입니다.
// Unity의 NavMeshAgent를 사용하므로, 바닥에는 NavMesh가 만들어져 있어야 합니다.
public class PersonMover : MonoBehaviour
{
    // 예전 왕복 이동용 시작점입니다. 지금 클릭 이동에서는 주로 InitializeIdle로 같은 위치를 넣습니다.
    [SerializeField] private Vector3 pointA;

    // 예전 왕복 이동용 도착점입니다.
    [SerializeField] private Vector3 pointB;

    // 사람이 초당 어느 정도 속도로 이동할지 정합니다.
    [SerializeField] private float moveSpeed = 2f;

    // 목적지에 이 거리만큼 가까워지면 "도착했다"고 봅니다.
    [SerializeField] private float stoppingDistance = 0.15f;

    // NavMeshAgent는 Unity가 제공하는 길찾기/이동 컴포넌트입니다.
    private NavMeshAgent agent;

    // 현재 이동 목표 지점입니다.
    private Vector3 routeTarget;

    // true이면 pointA와 pointB 사이를 자동 왕복합니다. 클릭 이동에서는 false로 둡니다.
    private bool patrolRouteEnabled;
    private bool hasDestinationCommand;

    public bool IsMoving => agent != null && (agent.pathPending || agent.remainingDistance > stoppingDistance);

    // 자동 왕복 이동을 시작하고 싶을 때 쓰는 초기화 함수입니다.
    // 지금 게임 흐름에서는 주로 클릭 이동을 쓰지만, 테스트용으로 남겨 두었습니다.
    public void InitializeRoute(Vector3 start, Vector3 end, float speed)
    {
        pointA = start;
        pointB = end;
        moveSpeed = speed;
        routeTarget = pointB;
        patrolRouteEnabled = true;

        EnsureAgent();
        WarpToNavMesh(pointA);
        MoveTo(routeTarget);
    }

    // 클릭 이동용 초기화 함수입니다.
    // 사람을 현재 위치에 두고, 목적지를 받을 때까지 자동으로 움직이지 않게 합니다.
    public void InitializeIdle(Vector3 position, float speed)
    {
        pointA = position;
        pointB = position;
        moveSpeed = speed;
        routeTarget = position;
        patrolRouteEnabled = false;

        EnsureAgent();
        WarpToNavMesh(position);
    }

    // Awake는 컴포넌트가 준비될 때 한 번 호출됩니다.
    // 여기서 NavMeshAgent가 없으면 자동으로 붙입니다.
    private void Awake()
    {
        EnsureAgent();
    }

    // Start는 첫 Update 직전에 한 번 호출됩니다.
    private void Start()
    {
        // 자동 왕복 모드일 때만 시작 목적지로 보냅니다.
        if (patrolRouteEnabled)
        {
            MoveTo(routeTarget == Vector3.zero ? pointB : routeTarget);
        }
    }

    // Update는 매 프레임 호출됩니다.
    private void Update()
    {
        if (!patrolRouteEnabled && hasDestinationCommand && agent != null && !agent.pathPending && agent.remainingDistance <= stoppingDistance)
        {
            hasDestinationCommand = false;

            PersonComponent person = GetComponent<PersonComponent>();
            if (person != null)
            {
                person.SetUnitStatus("\uB300\uAE30", "\uBA48\uCDA4");
            }
        }

        // 클릭 이동 모드라면 여기서 자동으로 새 목적지를 정하지 않습니다.
        if (!patrolRouteEnabled || agent == null || agent.pathPending || agent.remainingDistance > stoppingDistance)
        {
            return;
        }

        // 자동 왕복 모드에서는 A에 도착하면 B로, B에 도착하면 A로 목표를 바꿉니다.
        routeTarget = routeTarget == pointA ? pointB : pointA;
        MoveTo(routeTarget);
    }

    // 외부에서 "이 사람을 여기로 보내라"고 부르는 공개 함수입니다.
    // PersonClickMoveController가 우클릭 위치를 넘길 때 이 함수를 사용합니다.
    public void MoveToDestination(Vector3 destination)
    {
        patrolRouteEnabled = false;
        hasDestinationCommand = true;
        routeTarget = destination;
        MoveTo(destination);

        PersonComponent person = GetComponent<PersonComponent>();
        if (person != null)
        {
            person.SetUnitStatus("\uC774\uB3D9 \uC911", "\uD2B9\uC815 \uC9C0\uC5ED \uC774\uB3D9");
        }
    }

    // NavMeshAgent가 반드시 존재하도록 보장합니다.
    // null은 "아직 아무것도 연결되지 않았다"는 뜻으로 이해하면 됩니다.
    private void EnsureAgent()
    {
        if (agent != null)
        {
            return;
        }

        agent = GetComponent<NavMeshAgent>();
        if (agent == null)
        {
            agent = gameObject.AddComponent<NavMeshAgent>();
        }

        agent.speed = moveSpeed;
        agent.angularSpeed = 720f;
        agent.acceleration = 12f;
        agent.radius = 0.45f;
        agent.height = 1.5f;
        agent.stoppingDistance = stoppingDistance;
        agent.obstacleAvoidanceType = ObstacleAvoidanceType.HighQualityObstacleAvoidance;
        agent.avoidancePriority = Random.Range(30, 70);
    }

    // 실제 목적지를 NavMeshAgent에 전달합니다.
    private void MoveTo(Vector3 destination)
    {
        EnsureAgent();
        agent.speed = moveSpeed;

        // 클릭 지점이 NavMesh에서 살짝 벗어나도 주변 3m 안의 갈 수 있는 위치를 찾아 봅니다.
        if (NavMesh.SamplePosition(destination, out NavMeshHit hit, 3f, NavMesh.AllAreas))
        {
            agent.SetDestination(hit.position);
        }
    }

    // 오브젝트를 NavMesh 위의 위치로 즉시 옮깁니다.
    // Spawn 직후 사람이 NavMesh 밖에 걸치는 문제를 줄이기 위해 사용합니다.
    private void WarpToNavMesh(Vector3 position)
    {
        if (NavMesh.SamplePosition(position, out NavMeshHit hit, 3f, NavMesh.AllAreas))
        {
            agent.Warp(hit.position);
        }
    }
}
