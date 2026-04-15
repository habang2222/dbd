using UnityEngine;

public class MovementStateValidator : MonoBehaviour
{
    private const float BaseMaxObservedSpeed = 8f;
    private const float MaxAllowedSpeedMultiplier = 6f;
    private const float WindowGraceDistance = 1.2f;
    private const float ValidationWindowSeconds = 1f;

    private Vector3 lastPosition;
    private Vector3 windowStartPosition;
    private float windowStartTime;
    private bool hasLastPosition;

    private void LateUpdate()
    {
        PersonComponent person = GetComponent<PersonComponent>();
        if (person == null || AntiCheatService.IsFrozen(person))
        {
            lastPosition = transform.position;
            hasLastPosition = true;
            return;
        }

        if (!hasLastPosition)
        {
            lastPosition = transform.position;
            windowStartPosition = transform.position;
            windowStartTime = Time.time;
            hasLastPosition = true;
            return;
        }

        float elapsed = Mathf.Max(Time.time - windowStartTime, 0.0001f);
        float windowDistance = Vector3.Distance(windowStartPosition, transform.position);
        float speedMultiplier = GetAllowedSpeedMultiplier(person);
        float allowedDistance = (BaseMaxObservedSpeed * speedMultiplier * elapsed) + WindowGraceDistance;
        if (windowDistance > allowedDistance)
        {
            AntiCheatService.Punish(person, "impossible movement speed");
            windowStartPosition = transform.position;
            windowStartTime = Time.time;
            lastPosition = transform.position;
            return;
        }

        if (elapsed >= ValidationWindowSeconds)
        {
            windowStartPosition = transform.position;
            windowStartTime = Time.time;
        }

        lastPosition = transform.position;
    }

    private static float GetAllowedSpeedMultiplier(PersonComponent person)
    {
        float multiplier = 1f;
        if (person != null)
        {
            AuthorizedStatChangeTracker tracker = person.GetComponent<AuthorizedStatChangeTracker>();
            if (tracker != null)
            {
                multiplier = tracker.GetMoveSpeedMultiplier();
            }
        }

        return Mathf.Clamp(multiplier, 1f, MaxAllowedSpeedMultiplier);
    }
}
