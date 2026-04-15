using System.Collections.Generic;
using UnityEngine;

public class UnitCombatController : MonoBehaviour
{
    [SerializeField] private float attackRange = 2f;
    [SerializeField] private float attackInterval = 1f;
    [SerializeField] private float staminaCost = 10f;
    [SerializeField] private float staminaRecoveryPerSecond = 1f;
    [SerializeField] private float maxStamina = 100f;

    private PersonComponent person;
    private EnemyComponent enemy;
    private float attackTimer;
    private static readonly Dictionary<PersonComponent, float> retreatUntilTimes = new();

    private bool IsPerson => person != null;
    private PersonStats Stats => IsPerson ? person.Stats : enemy != null ? enemy.Stats : null;
    public static bool HasActiveCombat
    {
        get
        {
            PersonComponent selectedPerson = FindSelectedPerson();
            return selectedPerson != null && IsPersonInCombat(selectedPerson);
        }
    }

    public static bool IsPersonInCombat(PersonComponent targetPerson)
    {
        if (targetPerson == null || IsRetreating(targetPerson) || targetPerson.Stats.health <= 0f)
        {
            return false;
        }

        foreach (EnemyComponent candidate in FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None))
        {
            if (candidate == null || candidate.Stats.health <= 0f)
            {
                continue;
            }

            if (Vector3.Distance(targetPerson.transform.position, candidate.transform.position) <= 2f)
            {
                return true;
            }
        }

        return false;
    }

    public static void Retreat()
    {
        PersonComponent selectedPerson = FindSelectedPerson();
        if (selectedPerson == null)
        {
            return;
        }

        if (AntiCheatService.IsFrozen(selectedPerson))
        {
            return;
        }

        retreatUntilTimes[selectedPerson] = Time.time + 10f;
        MovementCommandService.TryStop(selectedPerson);

        foreach (BranchResource resource in FindObjectsByType<BranchResource>(FindObjectsSortMode.None))
        {
            resource.CancelGather(selectedPerson);
        }
    }

    public static bool IsRetreating(PersonComponent targetPerson)
    {
        if (targetPerson == null)
        {
            return false;
        }

        return retreatUntilTimes.TryGetValue(targetPerson, out float untilTime) && Time.time < untilTime;
    }

    private void Awake()
    {
        person = GetComponent<PersonComponent>();
        enemy = GetComponent<EnemyComponent>();
    }

    private void Update()
    {
        if (!GameAuthority.IsServerAuthority)
        {
            return;
        }

        PersonStats stats = Stats;
        if (stats == null || stats.health <= 0f)
        {
            return;
        }

        RecoverStamina(stats);
        attackTimer += Time.deltaTime;

        PersonStats targetStats = FindTargetStats();
        if (targetStats != null && targetStats.health > 0f && (person == null || !IsRetreating(person)))
        {
            if (person != null)
            {
                person.SetUnitStatus("\uC804\uD22C \uC911", "\uC804\uD22C");
                PersonMover mover = person.GetComponent<PersonMover>();
                if (mover != null)
                {
                    mover.StopForCombat();
                }
            }
        }

        if (person != null && IsRetreating(person))
        {
            return;
        }

        if (attackTimer < attackInterval)
        {
            return;
        }

        attackTimer = 0f;

        if (targetStats == null || targetStats.health <= 0f || stats.stamina < staminaCost)
        {
            return;
        }

        if (!CombatValidationService.TryApplyAttack(stats, targetStats, person, staminaCost, out string reason))
        {
            if (!string.IsNullOrWhiteSpace(reason))
            {
                Debug.LogWarning($"Combat rejected for {gameObject.name}: {reason}");
            }

            return;
        }

        if (UnitListPanel.Instance != null)
        {
            UnitListPanel.Instance.RefreshList();
        }
    }

    private void RecoverStamina(PersonStats stats)
    {
        PersonMover mover = GetComponent<PersonMover>();
        if (mover != null && mover.IsRunning)
        {
            return;
        }

        stats.stamina = Mathf.Min(maxStamina, stats.stamina + staminaRecoveryPerSecond * Time.deltaTime);
    }

    private PersonStats FindTargetStats()
    {
        if (IsPerson)
        {
            EnemyComponent targetEnemy = FindClosestEnemy();
            return targetEnemy != null ? targetEnemy.Stats : null;
        }

        PersonComponent targetPerson = FindClosestPerson();
        return targetPerson != null ? targetPerson.Stats : null;
    }

    private EnemyComponent FindClosestEnemy()
    {
        EnemyComponent best = null;
        float bestDistance = attackRange;

        foreach (EnemyComponent candidate in FindObjectsByType<EnemyComponent>(FindObjectsSortMode.None))
        {
            if (candidate == null || candidate.Stats.health <= 0f)
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

    private PersonComponent FindClosestPerson()
    {
        PersonComponent best = null;
        float bestDistance = attackRange;

        foreach (PersonComponent candidate in FindObjectsByType<PersonComponent>(FindObjectsSortMode.None))
        {
            if (candidate == null || candidate.Stats.health <= 0f)
            {
                continue;
            }

            if (IsRetreating(candidate))
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
}
