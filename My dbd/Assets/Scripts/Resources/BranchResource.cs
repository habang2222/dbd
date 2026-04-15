using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class BranchResource : MonoBehaviour
{
    [SerializeField] private string itemId = "branch";
    [SerializeField] private string displayName = "Branch";
    [SerializeField] private float gatherDuration = 1f;
    [SerializeField] private int gatherAmount = 1;
    [SerializeField] private float gatherRange = 1f;
    [SerializeField] private float shrinkDuration = 0.8f;
    [SerializeField] private List<PersonInventoryItem> extraYields = new();

    private bool isGathering;
    private bool isCollected;
    private bool isCollectionAnimationActive;
    private Vector3 originalScale;
    private Renderer cachedRenderer;
    private Color originalColor;
    private Canvas gatherCanvas;
    private Text gatherText;
    private float gatherMessageHideAt;
    public bool IsCollectionAnimationActive => isCollectionAnimationActive;

    private void Awake()
    {
        originalScale = transform.localScale;
        if (GetComponent<ResourceIntegrityValidator>() == null)
        {
            gameObject.AddComponent<ResourceIntegrityValidator>();
        }

        cachedRenderer = GetComponent<Renderer>();
        if (cachedRenderer != null)
        {
            originalColor = cachedRenderer.material.color;
        }
    }

    public void TryStartGather()
    {
        if (isGathering || isCollected)
        {
            return;
        }

        PersonComponent gatherer = FindSelectedPerson();
        if (gatherer == null || gatherer.Stats.health <= 0f)
        {
            Debug.Log($"Select a person before gathering {displayName}.");
            return;
        }

        StartCoroutine(GatherWhenClose(gatherer));
    }

    public IEnumerator GatherWith(PersonComponent gatherer)
    {
        if (isGathering || isCollected)
        {
            yield break;
        }

        yield return GatherWhenClose(gatherer);
    }

    public void Configure(string newItemId, string newDisplayName)
    {
        itemId = newItemId;
        displayName = newDisplayName;
        gameObject.name = newDisplayName;
    }

    public void Configure(string newItemId, string newDisplayName, float newGatherDuration, int newGatherAmount = 1)
    {
        itemId = newItemId;
        displayName = newDisplayName;
        gatherDuration = Mathf.Max(0.05f, newGatherDuration);
        gatherAmount = Mathf.Max(1, newGatherAmount);
        gameObject.name = newDisplayName;
    }

    public void ConfigureYields(string newDisplayName, float newGatherDuration, IEnumerable<PersonInventoryItem> yields)
    {
        itemId = string.Empty;
        displayName = newDisplayName;
        gatherDuration = Mathf.Max(0.05f, newGatherDuration);
        extraYields = new List<PersonInventoryItem>(yields);
        gameObject.name = newDisplayName;
    }

    public void SetGatherHighlighted(bool highlighted)
    {
        if (cachedRenderer == null)
        {
            cachedRenderer = GetComponent<Renderer>();
        }

        if (cachedRenderer == null)
        {
            return;
        }

        cachedRenderer.material.color = highlighted
            ? new Color(1f, 0.84f, 0.18f, 1f)
            : originalColor;
    }

    private IEnumerator GatherWhenClose(PersonComponent gatherer)
    {
        if (isGathering || isCollected)
        {
            yield break;
        }

        isGathering = true;
        float moveWaitStartedAt = Time.time;
        float maxMoveWaitSeconds = Mathf.Max(12f, Vector3.Distance(gatherer.transform.position, transform.position) * 3f);

        if (!MovementCommandService.TryMove(gatherer, transform.position, ActionWindow.RunEnabled, "채집 지점 이동"))
        {
            CancelGather(gatherer);
            yield break;
        }

        while (gatherer != null && !isCollected && GetDistanceToGatherer(gatherer) > gatherRange)
        {
            if (gatherer.Stats.health <= 0f || UnitCombatController.IsPersonInCombat(gatherer))
            {
                CancelGather(gatherer);
                yield break;
            }

            if (Time.time - moveWaitStartedAt > maxMoveWaitSeconds)
            {
                CancelGather(gatherer);
                yield break;
            }

            yield return null;
        }

        if (gatherer == null || gatherer.Stats.health <= 0f || isCollected)
        {
            CancelGather(gatherer);
            yield break;
        }

        gatherer.SetUnitStatus("\uCC44\uC9D1 \uC911", "\uCC44\uC9D1 \uC911");
        float totalGatherSeconds = gatherDuration * GetScaleGatherMultiplier();
        float gatherTimer = 0f;
        while (gatherTimer < totalGatherSeconds)
        {
            if (gatherer == null
                || gatherer.Stats.health <= 0f
                || AntiCheatService.IsFrozen(gatherer)
                || UnitCombatController.IsPersonInCombat(gatherer))
            {
                CancelGather(gatherer);
                yield break;
            }

            gatherTimer += Time.deltaTime;
            ShowGatherMessage("채집 중\n" + Mathf.CeilToInt(Mathf.Max(0f, totalGatherSeconds - gatherTimer)) + "초");
            yield return null;
        }

        if (gatherer == null
            || gatherer.Stats.health <= 0f
            || AntiCheatService.IsFrozen(gatherer)
            || UnitCombatController.IsPersonInCombat(gatherer))
        {
            CancelGather(gatherer);
            yield break;
        }

        bool rewardApplied = true;
        if (gatherer != null)
        {
            if (!string.IsNullOrWhiteSpace(itemId))
            {
                rewardApplied &= GrantAndAddItem(gatherer, itemId, gatherAmount);
            }

            foreach (PersonInventoryItem yieldItem in extraYields)
            {
                if (yieldItem != null)
                {
                    rewardApplied &= GrantAndAddItem(gatherer, yieldItem.itemId, yieldItem.count);
                }
            }

            if (!rewardApplied)
            {
                CancelGather(gatherer);
                yield break;
            }

            gatherer.SetUnitStatus("Idle", "None");
        }

        ShowGatherMessage("채집 완료\n" + BuildCollectedMessage(), 2.2f);
        if (gatherCanvas != null)
        {
            gatherCanvas.transform.SetParent(null, true);
            Destroy(gatherCanvas.gameObject, 2.4f);
        }

        isCollected = true;
        isCollectionAnimationActive = true;
        SetGatherHighlighted(false);
        float timer = 0f;
        while (timer < shrinkDuration)
        {
            timer += Time.deltaTime;
            float progress = Mathf.Clamp01(timer / shrinkDuration);
            transform.localScale = Vector3.Lerp(originalScale, Vector3.zero, progress);
            yield return null;
        }

        Destroy(gameObject);
    }

    private void Update()
    {
        if (gatherCanvas != null && gatherCanvas.gameObject.activeSelf && Time.time >= gatherMessageHideAt)
        {
            gatherCanvas.gameObject.SetActive(false);
        }

        if (gatherCanvas != null && gatherCanvas.gameObject.activeSelf)
        {
            UpdateGatherCanvasTransform();
        }
    }

    private static bool GrantAndAddItem(PersonComponent gatherer, string grantItemId, int grantCount)
    {
        if (!ServerItemGrantLedger.TryBeginGrant(gatherer, grantItemId, grantCount))
        {
            return false;
        }

        try
        {
            return InventoryService.AddItem(gatherer, grantItemId, grantCount);
        }
        finally
        {
            ServerItemGrantLedger.EndGrant();
        }
    }

    private float GetScaleGatherMultiplier()
    {
        if (originalScale == Vector3.zero)
        {
            return 1f;
        }

        float currentAverage = (transform.localScale.x + transform.localScale.y + transform.localScale.z) / 3f;
        float originalAverage = (originalScale.x + originalScale.y + originalScale.z) / 3f;
        return Mathf.Max(0.1f, currentAverage / Mathf.Max(0.01f, originalAverage));
    }

    public void CancelGather(PersonComponent gatherer)
    {
        isGathering = false;
        SetGatherHighlighted(false);
        HideGatherMessage();
        if (gatherer != null && gatherer.Stats.health > 0f && !UnitCombatController.IsPersonInCombat(gatherer))
        {
            gatherer.SetUnitStatus("Idle", "None");
        }
    }

    private string BuildCollectedMessage()
    {
        if (!string.IsNullOrWhiteSpace(itemId))
        {
            return displayName + " x" + gatherAmount;
        }

        List<string> parts = new();
        foreach (PersonInventoryItem yieldItem in extraYields)
        {
            if (yieldItem != null)
            {
                parts.Add(yieldItem.itemId + " x" + yieldItem.count);
            }
        }

        return parts.Count > 0 ? string.Join(", ", parts) : displayName;
    }

    private void ShowGatherMessage(string message, float visibleSeconds = 0.35f)
    {
        EnsureGatherCanvas();
        gatherText.text = message;
        gatherMessageHideAt = Time.time + visibleSeconds;
        gatherCanvas.gameObject.SetActive(true);
    }

    private void HideGatherMessage()
    {
        if (gatherCanvas != null)
        {
            gatherCanvas.gameObject.SetActive(false);
        }
    }

    private void EnsureGatherCanvas()
    {
        if (gatherCanvas != null)
        {
            return;
        }

        gatherCanvas = new GameObject("Gather Progress Canvas").AddComponent<Canvas>();
        gatherCanvas.renderMode = RenderMode.WorldSpace;
        gatherCanvas.transform.localScale = Vector3.one * 0.01f;
        UpdateGatherCanvasTransform();

        RectTransform canvasRect = gatherCanvas.GetComponent<RectTransform>();
        canvasRect.sizeDelta = new Vector2(260f, 90f);

        GameObject textObject = new GameObject("Gather Progress Text");
        textObject.transform.SetParent(gatherCanvas.transform, false);
        RectTransform textRect = textObject.AddComponent<RectTransform>();
        textRect.anchorMin = Vector2.zero;
        textRect.anchorMax = Vector2.one;
        textRect.offsetMin = Vector2.zero;
        textRect.offsetMax = Vector2.zero;

        gatherText = textObject.AddComponent<Text>();
        gatherText.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        gatherText.fontSize = 22;
        gatherText.fontStyle = FontStyle.Bold;
        gatherText.alignment = TextAnchor.MiddleCenter;
        gatherText.color = Color.white;
        gatherCanvas.gameObject.SetActive(false);
    }

    private void UpdateGatherCanvasTransform()
    {
        gatherCanvas.transform.position = transform.position + Vector3.up * 2.2f;
        Camera camera = Camera.main;
        if (camera != null)
        {
            gatherCanvas.transform.rotation = camera.transform.rotation;
        }
        else
        {
            gatherCanvas.transform.rotation = Quaternion.Euler(65f, 0f, 0f);
        }
    }

    private float GetDistanceToGatherer(PersonComponent gatherer)
    {
        Collider resourceCollider = GetComponent<Collider>();
        Vector3 gathererPosition = gatherer.transform.position;
        if (resourceCollider == null)
        {
            Vector3 resourcePosition = transform.position;
            resourcePosition.y = gathererPosition.y;
            return Vector3.Distance(gathererPosition, resourcePosition);
        }

        Vector3 closestPoint = resourceCollider.ClosestPoint(gathererPosition);
        closestPoint.y = gathererPosition.y;
        return Vector3.Distance(gathererPosition, closestPoint);
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
