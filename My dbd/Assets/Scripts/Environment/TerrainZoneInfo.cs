using UnityEngine;

public class TerrainZoneInfo : MonoBehaviour
{
    [SerializeField] private string zoneName;
    [SerializeField] private bool walkable = true;
    [SerializeField] private float speedMultiplier = 1f;

    public string ZoneName => zoneName;
    public bool Walkable => walkable;
    public float SpeedMultiplier => speedMultiplier;

    public void Configure(string newZoneName, bool newWalkable, float newSpeedMultiplier)
    {
        zoneName = newZoneName;
        walkable = newWalkable;
        speedMultiplier = Mathf.Max(0.1f, newSpeedMultiplier);
    }

    public bool Contains(Vector3 worldPosition)
    {
        Vector3 center = transform.position;
        Vector3 scale = transform.localScale;
        float halfX = Mathf.Max(0.1f, scale.x * 0.5f);
        float halfZ = Mathf.Max(0.1f, scale.z * 0.5f);

        return worldPosition.x >= center.x - halfX
            && worldPosition.x <= center.x + halfX
            && worldPosition.z >= center.z - halfZ
            && worldPosition.z <= center.z + halfZ;
    }

    public static TerrainZoneInfo FindAt(Vector3 worldPosition)
    {
        TerrainZoneInfo best = null;
        float bestArea = float.MaxValue;

        foreach (TerrainZoneInfo zone in FindObjectsByType<TerrainZoneInfo>(FindObjectsSortMode.None))
        {
            if (!zone.Contains(worldPosition))
            {
                continue;
            }

            Vector3 scale = zone.transform.localScale;
            float area = Mathf.Max(0.01f, scale.x * scale.z);
            if (area < bestArea)
            {
                best = zone;
                bestArea = area;
            }
        }

        return best;
    }

    public static bool CanStandAt(Vector3 worldPosition)
    {
        return EnvironmentRuntimeBootstrap.CanStandAt(worldPosition);
    }

    public static float GetSpeedMultiplier(Vector3 worldPosition)
    {
        return EnvironmentRuntimeBootstrap.GetTerrainSpeedMultiplier(worldPosition);
    }
}
