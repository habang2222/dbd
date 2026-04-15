using UnityEngine;
using UnityEngine.AI;

public class EnemyWanderer : MonoBehaviour
{
    [SerializeField] private Vector3 patrolCenter;
    [SerializeField] private float patrolRadius = 8f;
    [SerializeField] private float moveSpeed = 1.8f;
    [SerializeField] private float stoppingDistance = 0.4f;
    [SerializeField] private float chaseRange = 5f;
    [SerializeField] private float combatRange = 2f;

    private NavMeshAgent agent;
    private Vector3 currentPatrolTarget;

    public void Initialize(Vector3 center, float radius)
    {
        patrolCenter = center;
        patrolRadius = radius;
        EnsureAgent();
        WarpToNavMesh(transform.position);
        PickNextDestination();
    }

    private void Awake()
    {
        if (patrolCenter == Vector3.zero)
        {
            patrolCenter = transform.position;
        }

        EnsureAgent();
        WarpToNavMesh(transform.position);
    }

    private void Update()
    {
        PersonComponent target = FindClosestLivingPerson();
        if (target != null)
        {
            float distance = Vector3.Distance(transform.position, target.transform.position);
            if (distance <= combatRange)
            {
                StopForCombat();
                return;
            }

            if (distance <= chaseRange)
            {
                MoveTo(target.transform.position);
                return;
            }
        }

        if (agent == null || agent.pathPending || agent.remainingDistance > stoppingDistance)
        {
            return;
        }

        PickNextDestination();
    }

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
        agent.acceleration = 10f;
        agent.radius = 0.45f;
        agent.height = 1.5f;
        agent.stoppingDistance = stoppingDistance;
        agent.obstacleAvoidanceType = ObstacleAvoidanceType.HighQualityObstacleAvoidance;
        agent.avoidancePriority = Random.Range(20, 50);
    }

    private void PickNextDestination()
    {
        EnsureAgent();
        if (!EnsureOnNavMesh())
        {
            return;
        }

        for (int i = 0; i < 10; i++)
        {
            Vector2 offset = Random.insideUnitCircle * patrolRadius;
            Vector3 candidate = patrolCenter + new Vector3(offset.x, 0f, offset.y);
            if (NavMesh.SamplePosition(candidate, out NavMeshHit hit, 3f, NavMesh.AllAreas))
            {
                currentPatrolTarget = hit.position;
                MoveTo(currentPatrolTarget);
                return;
            }
        }
    }

    private void MoveTo(Vector3 destination)
    {
        EnsureAgent();
        if (!EnsureOnNavMesh())
        {
            return;
        }

        if (NavMesh.SamplePosition(destination, out NavMeshHit hit, 3f, NavMesh.AllAreas))
        {
            agent.isStopped = false;
            agent.SetDestination(hit.position);
        }
    }

    private void StopForCombat()
    {
        EnsureAgent();
        if (!EnsureOnNavMesh())
        {
            return;
        }

        agent.isStopped = true;
        agent.ResetPath();
    }

    private bool EnsureOnNavMesh()
    {
        if (agent == null)
        {
            return false;
        }

        if (agent.isOnNavMesh)
        {
            return true;
        }

        return WarpToNavMesh(transform.position);
    }

    private bool WarpToNavMesh(Vector3 position)
    {
        if (agent == null)
        {
            return false;
        }

        if (NavMesh.SamplePosition(position, out NavMeshHit hit, 6f, NavMesh.AllAreas))
        {
            agent.Warp(hit.position);
            return true;
        }

        return false;
    }

    private PersonComponent FindClosestLivingPerson()
    {
        PersonComponent best = null;
        float bestDistance = chaseRange;

        foreach (PersonComponent candidate in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (candidate == null || candidate.Stats.health <= 0f)
            {
                continue;
            }

            if (UnitCombatController.IsRetreating(candidate))
            {
                continue;
            }

            float distance = Vector3.Distance(transform.position, candidate.transform.position);
            if (distance <= bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }

        return best;
    }
}
