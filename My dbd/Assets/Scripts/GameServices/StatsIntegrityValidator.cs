using UnityEngine;

public class StatsIntegrityValidator : MonoBehaviour
{
    private const float MaxHealth = 500f;
    private const float MaxStrength = 250f;
    private const float MaxStamina = 150f;
    private const float MaxHealthIncreasePerSecond = 5f;
    private const float MaxStrengthIncreasePerSecond = 0.25f;
    private const float MaxStaminaIncreasePerSecond = 8f;

    private float lastHealth;
    private float lastStrength;
    private float lastStamina;
    private float lastCheckTime;
    private bool initialized;

    private void LateUpdate()
    {
        PersonComponent person = GetComponent<PersonComponent>();
        PersonStats stats = person != null ? person.Stats : null;
        if (stats == null || AntiCheatService.IsFrozen(person))
        {
            Snapshot(stats);
            return;
        }

        if (!initialized)
        {
            Snapshot(stats);
            return;
        }

        float elapsed = Mathf.Max(Time.time - lastCheckTime, 0.0001f);
        if (!IsFinite(stats) || stats.health > MaxHealth || stats.strength > MaxStrength || stats.stamina > MaxStamina)
        {
            AntiCheatService.Punish(person, "stats outside allowed range");
            Snapshot(stats);
            return;
        }

        AuthorizedStatChangeTracker tracker = person.GetComponent<AuthorizedStatChangeTracker>();
        float healthIncrease = stats.health - lastHealth;
        float strengthIncrease = stats.strength - lastStrength;
        float staminaIncrease = stats.stamina - lastStamina;

        if (healthIncrease > MaxHealthIncreasePerSecond * elapsed
            && (tracker == null || !tracker.ConsumeHealthIncrease(healthIncrease)))
        {
            AntiCheatService.Punish(person, "impossible health increase");
        }

        if (strengthIncrease > MaxStrengthIncreasePerSecond * elapsed
            && (tracker == null || !tracker.ConsumeStrengthIncrease(strengthIncrease)))
        {
            AntiCheatService.Punish(person, "impossible strength increase");
        }

        if (staminaIncrease > MaxStaminaIncreasePerSecond * elapsed
            && (tracker == null || !tracker.ConsumeStaminaIncrease(staminaIncrease)))
        {
            AntiCheatService.Punish(person, "impossible stamina increase");
        }

        Snapshot(stats);
    }

    private void Snapshot(PersonStats stats)
    {
        if (stats == null)
        {
            initialized = false;
            return;
        }

        lastHealth = stats.health;
        lastStrength = stats.strength;
        lastStamina = stats.stamina;
        lastCheckTime = Time.time;
        initialized = true;
    }

    private static bool IsFinite(PersonStats stats)
    {
        return float.IsFinite(stats.health)
            && float.IsFinite(stats.strength)
            && float.IsFinite(stats.stamina);
    }
}
