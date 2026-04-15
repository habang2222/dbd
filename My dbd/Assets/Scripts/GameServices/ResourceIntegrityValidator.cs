using UnityEngine;

public class ResourceIntegrityValidator : MonoBehaviour
{
    private const float MaxResourceScale = 8f;
    private const float MinResourceScale = 0.02f;

    private Vector3 lastPosition;
    private bool initialized;

    private void LateUpdate()
    {
        BranchResource resource = GetComponent<BranchResource>();
        if (resource == null)
        {
            return;
        }

        if (resource.IsCollectionAnimationActive)
        {
            return;
        }

        if (!initialized)
        {
            lastPosition = transform.position;
            initialized = true;
            return;
        }

        if (!AntiCheatService.IsFinite(transform.position) || !IsScaleAllowed(transform.localScale))
        {
            Debug.LogWarning($"ANTI-CHEAT: invalid resource transform on {gameObject.name}");
            Destroy(gameObject);
            return;
        }

        if (Vector3.Distance(lastPosition, transform.position) > 0.5f)
        {
            Debug.LogWarning($"ANTI-CHEAT: resource moved unexpectedly on {gameObject.name}");
            transform.position = lastPosition;
        }
    }

    private static bool IsScaleAllowed(Vector3 scale)
    {
        return AntiCheatService.IsFinite(scale)
            && scale.x >= MinResourceScale
            && scale.y >= MinResourceScale
            && scale.z >= MinResourceScale
            && scale.x <= MaxResourceScale
            && scale.y <= MaxResourceScale
            && scale.z <= MaxResourceScale;
    }
}
