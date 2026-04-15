using UnityEngine;
using UnityEngine.AI;

public class UnitDeathShrink : MonoBehaviour
{
    [SerializeField] private float shrinkDuration = 1.5f;

    private PersonComponent person;
    private EnemyComponent enemy;
    private Vector3 originalScale;
    private float deathTimer;
    private bool isDying;

    private PersonStats Stats => person != null ? person.Stats : enemy != null ? enemy.Stats : null;

    private void Awake()
    {
        person = GetComponent<PersonComponent>();
        enemy = GetComponent<EnemyComponent>();
        originalScale = transform.localScale;
    }

    private void Update()
    {
        PersonStats stats = Stats;
        if (stats == null || stats.health > 0f)
        {
            return;
        }

        if (!isDying)
        {
            isDying = true;
            StopMovement();
        }

        deathTimer += Time.deltaTime;
        float progress = Mathf.Clamp01(deathTimer / shrinkDuration);
        transform.localScale = Vector3.Lerp(originalScale, Vector3.zero, progress);

        if (progress >= 1f)
        {
            Destroy(gameObject);
        }
    }

    private void StopMovement()
    {
        PersonMover personMover = GetComponent<PersonMover>();
        if (personMover != null)
        {
            personMover.StopForCombat();
        }

        NavMeshAgent agent = GetComponent<NavMeshAgent>();
        if (agent != null && agent.isOnNavMesh)
        {
            agent.isStopped = true;
            agent.ResetPath();
        }

        Collider collider = GetComponent<Collider>();
        if (collider != null)
        {
            collider.enabled = false;
        }
    }
}
