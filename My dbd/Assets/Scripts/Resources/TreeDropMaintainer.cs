using UnityEngine;

public class TreeDropMaintainer : MonoBehaviour
{
    [SerializeField] private float radius = 10f;
    [SerializeField] private int maxLeaves = 10;
    [SerializeField] private int maxBranches = 10;
    [SerializeField] private float refillInterval = 3f;

    private float nextRefillTime;

    public void Configure(float newRadius)
    {
        radius = newRadius;
    }

    private void Start()
    {
        RefillNow();
    }

    private void Update()
    {
        if (Time.time < nextRefillTime)
        {
            return;
        }

        nextRefillTime = Time.time + refillInterval;
        RefillNow();
    }

    public void RefillNow()
    {
        int leafCount = CountNearby("Leaf");
        int branchCount = CountNearby("Branch");

        while (leafCount < maxLeaves)
        {
            ResourceRuntimeBootstrap.CreateTreeDrop("leaf_" + Random.Range(1, 7), "Leaf", GetRandomNearbyPosition());
            leafCount++;
        }

        while (branchCount < maxBranches)
        {
            ResourceRuntimeBootstrap.CreateTreeDrop("branch_" + Random.Range(1, 6), "Branch", GetRandomNearbyPosition());
            branchCount++;
        }
    }

    private int CountNearby(string namePrefix)
    {
        int count = 0;
        foreach (BranchResource resource in FindObjectsByType<BranchResource>(FindObjectsSortMode.None))
        {
            if (resource == null || !resource.gameObject.name.StartsWith(namePrefix))
            {
                continue;
            }

            if (Vector3.Distance(transform.position, resource.transform.position) <= radius)
            {
                count++;
            }
        }

        return count;
    }

    private Vector3 GetRandomNearbyPosition()
    {
        Vector2 circle = Random.insideUnitCircle * radius;
        return new Vector3(transform.position.x + circle.x, 0.18f, transform.position.z + circle.y);
    }
}
