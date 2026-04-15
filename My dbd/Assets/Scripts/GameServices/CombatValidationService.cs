using UnityEngine;

public static class CombatValidationService
{
    private const float MaxDamagePerHit = 250f;
    private const float MaxHealth = 500f;
    private const float MaxStamina = 150f;

    public static bool TryApplyAttack(PersonStats attackerStats, PersonStats targetStats, PersonComponent attackerPerson, float staminaCost, out string reason)
    {
        reason = string.Empty;
        if (attackerStats == null || targetStats == null)
        {
            reason = "missing combat stats";
            AntiCheatService.Punish(attackerPerson, reason);
            return false;
        }

        if (!IsValidStats(attackerStats) || !IsValidStats(targetStats))
        {
            reason = "invalid combat stats";
            AntiCheatService.Punish(attackerPerson, reason);
            return false;
        }

        if (staminaCost < 0f || staminaCost > MaxStamina)
        {
            reason = "invalid stamina cost";
            AntiCheatService.Punish(attackerPerson, reason);
            return false;
        }

        if (attackerStats.stamina < staminaCost)
        {
            reason = "attack without stamina";
            return false;
        }

        float damage = Mathf.Clamp(attackerStats.strength, 0f, MaxDamagePerHit);
        attackerStats.stamina = Mathf.Clamp(attackerStats.stamina - staminaCost, 0f, MaxStamina);
        targetStats.health = Mathf.Clamp(targetStats.health - damage, 0f, MaxHealth);
        return true;
    }

    public static void ClampStats(PersonStats stats)
    {
        if (stats == null)
        {
            return;
        }

        stats.health = Mathf.Clamp(stats.health, 0f, MaxHealth);
        stats.strength = Mathf.Clamp(stats.strength, 0f, MaxDamagePerHit);
        stats.stamina = Mathf.Clamp(stats.stamina, 0f, MaxStamina);
    }

    private static bool IsValidStats(PersonStats stats)
    {
        return float.IsFinite(stats.health)
            && float.IsFinite(stats.strength)
            && float.IsFinite(stats.stamina)
            && stats.health >= 0f
            && stats.health <= MaxHealth
            && stats.strength >= 0f
            && stats.strength <= MaxDamagePerHit
            && stats.stamina >= 0f
            && stats.stamina <= MaxStamina;
    }
}
