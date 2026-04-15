using UnityEngine;

public class TerrainSurfaceEnforcer : MonoBehaviour
{
    private static readonly Color DirtColor = new(0.24f, 0.48f, 0.20f, 1f);
    private Renderer cachedRenderer;
    private float enforceUntil;

    private void Awake()
    {
        cachedRenderer = GetComponent<Renderer>();
        enforceUntil = Time.time + 8f;
        ApplyDirt();
    }

    private void Update()
    {
        if (Time.time > enforceUntil)
        {
            enabled = false;
            return;
        }

        ApplyDirt();
    }

    private void ApplyDirt()
    {
        if (cachedRenderer == null)
        {
            cachedRenderer = GetComponent<Renderer>();
        }

        if (cachedRenderer != null)
        {
            cachedRenderer.material.color = DirtColor;
        }
    }
}
