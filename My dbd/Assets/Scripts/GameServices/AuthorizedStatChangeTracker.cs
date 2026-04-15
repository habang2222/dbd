using UnityEngine;

public class AuthorizedStatChangeTracker : MonoBehaviour
{
    private float allowedHealthIncrease;
    private float allowedStrengthIncrease;
    private float allowedStaminaIncrease;
    private float activeMoveSpeedMultiplier = 1f;

    public void AllowStatIncrease(float health, float strength, float stamina)
    {
        allowedHealthIncrease += Mathf.Max(0f, health);
        allowedStrengthIncrease += Mathf.Max(0f, strength);
        allowedStaminaIncrease += Mathf.Max(0f, stamina);
    }

    public void AllowMoveSpeedMultiplierBonus(float multiplierBonus)
    {
        activeMoveSpeedMultiplier = Mathf.Max(activeMoveSpeedMultiplier, 1f + Mathf.Max(0f, multiplierBonus));
    }

    public void SetMoveSpeedMultiplier(float multiplier)
    {
        activeMoveSpeedMultiplier = Mathf.Clamp(multiplier, 1f, 6f);
    }

    public bool ConsumeHealthIncrease(float amount)
    {
        return Consume(ref allowedHealthIncrease, amount);
    }

    public bool ConsumeStrengthIncrease(float amount)
    {
        return Consume(ref allowedStrengthIncrease, amount);
    }

    public bool ConsumeStaminaIncrease(float amount)
    {
        return Consume(ref allowedStaminaIncrease, amount);
    }

    public float GetMoveSpeedMultiplier()
    {
        return activeMoveSpeedMultiplier;
    }

    private static bool Consume(ref float allowed, float amount)
    {
        if (amount <= 0f)
        {
            return true;
        }

        if (allowed + 0.001f < amount)
        {
            return false;
        }

        allowed = Mathf.Max(0f, allowed - amount);
        return true;
    }
}
