using UnityEngine;
using UnityEngine.EventSystems;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

public class ActionCommandController : MonoBehaviour
{
    [SerializeField] private Camera targetCamera;
    [SerializeField] private float maxRayDistance = 500f;

    private Vector2 dragStart;
    private Vector3 dragWorldStart;
    private Vector3 dragWorldEnd;
    private LineRenderer dragLine;
    private bool isDraggingGather;
    private Coroutine gatherQueue;
    private PersonComponent currentGatherPerson;
    private readonly List<BranchResource> highlightedBranches = new();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<ActionCommandController>() != null)
        {
            return;
        }

        GameObject controllerObject = new GameObject("Action Command Controller");
        controllerObject.AddComponent<ActionCommandController>();
    }

    private void Awake()
    {
        targetCamera = Camera.main;
        CreateDragVisual();
    }

    private void Update()
    {
        PersonComponent selectedPerson = FindSelectedPerson();
        if (currentGatherPerson != null && (selectedPerson != currentGatherPerson || ActionWindow.CurrentAction != UnitActionMode.Gather))
        {
            CancelHighlightedGathers(currentGatherPerson);
            ClearGatherHighlights();
            currentGatherPerson = null;
            if (gatherQueue != null)
            {
                StopCoroutine(gatherQueue);
                gatherQueue = null;
            }
        }

        if (ActionWindow.CurrentAction == UnitActionMode.Gather)
        {
            UpdateGatherDrag();
            return;
        }

        if (!Input.GetMouseButtonDown(0) || IsPointerOverUi())
        {
            return;
        }

        PersonComponent person = selectedPerson;
        if (person == null)
        {
            return;
        }

        if (ActionWindow.CurrentAction == UnitActionMode.Stop)
        {
            StopPerson(person);
            return;
        }

        if (targetCamera == null)
        {
            targetCamera = Camera.main;
        }

        if (targetCamera == null)
        {
            return;
        }

        Ray ray = targetCamera.ScreenPointToRay(Input.mousePosition);
        if (!Physics.Raycast(ray, out RaycastHit hit, maxRayDistance))
        {
            return;
        }

        if (hit.collider.GetComponentInParent<PersonComponent>() != null || hit.collider.gameObject.name != "Ground")
        {
            return;
        }

        MovementCommandService.TryMove(person, hit.point, ActionWindow.RunEnabled);
    }

    private static void StopPerson(PersonComponent person)
    {
        MovementCommandService.TryStop(person);
    }

    private void UpdateGatherDrag()
    {
        if (Input.GetMouseButtonDown(0))
        {
            if (IsPointerOverUi())
            {
                return;
            }

            if (!TryGetGroundPoint(Input.mousePosition, out dragWorldStart))
            {
                return;
            }

            dragStart = Input.mousePosition;
            dragWorldEnd = dragWorldStart;
            isDraggingGather = true;
            dragLine.gameObject.SetActive(true);
            SetGroundDragRect(dragWorldStart, dragWorldEnd);
            return;
        }

        if (isDraggingGather && Input.GetMouseButton(0))
        {
            if (TryGetGroundPoint(Input.mousePosition, out dragWorldEnd))
            {
                SetGroundDragRect(dragWorldStart, dragWorldEnd);
            }

            return;
        }

        if (!isDraggingGather || !Input.GetMouseButtonUp(0))
        {
            return;
        }

        Vector2 dragEnd = Input.mousePosition;
        isDraggingGather = false;
        dragLine.gameObject.SetActive(false);

        if (Vector2.Distance(dragStart, dragEnd) < 8f)
        {
            BranchResource branch = FindBranchAtPointer(dragEnd);
            if (branch != null)
            {
                StartGatherQueue(new[] { branch });
            }
            return;
        }

        Bounds selection = GetGroundSelectionBounds(dragWorldStart, dragWorldEnd);
        List<BranchResource> selectedBranches = new();
        foreach (BranchResource branch in FindObjectsByType<BranchResource>(FindObjectsSortMode.None))
        {
            Vector3 branchPosition = branch.transform.position;
            if (selection.Contains(new Vector3(branchPosition.x, selection.center.y, branchPosition.z)))
            {
                selectedBranches.Add(branch);
            }
        }

        StartGatherQueue(selectedBranches);
    }

    private BranchResource FindBranchAtPointer(Vector2 screenPoint)
    {
        if (targetCamera == null)
        {
            targetCamera = Camera.main;
        }

        if (targetCamera == null)
        {
            return null;
        }

        Ray ray = targetCamera.ScreenPointToRay(screenPoint);
        RaycastHit[] hits = Physics.RaycastAll(ray, maxRayDistance);
        foreach (RaycastHit hit in hits)
        {
            BranchResource branch = hit.collider.GetComponentInParent<BranchResource>();
            if (branch != null)
            {
                return branch;
            }
        }

        return null;
    }

    private void StartGatherQueue(IEnumerable<BranchResource> branches)
    {
        PersonComponent person = FindSelectedPerson();
        if (person == null)
        {
            return;
        }

        List<BranchResource> orderedBranches = branches
            .Where(branch => branch != null)
            .OrderBy(branch => Vector3.Distance(person.transform.position, branch.transform.position))
            .ToList();

        if (orderedBranches.Count == 0)
        {
            return;
        }

        if (gatherQueue != null)
        {
            CancelHighlightedGathers(person);
            StopCoroutine(gatherQueue);
            gatherQueue = null;
        }

        ClearGatherHighlights();
        currentGatherPerson = person;
        foreach (BranchResource branch in orderedBranches)
        {
            branch.SetGatherHighlighted(true);
            highlightedBranches.Add(branch);
        }

        gatherQueue = StartCoroutine(GatherBranchesInOrder(person, orderedBranches));
    }

    private IEnumerator GatherBranchesInOrder(PersonComponent person, List<BranchResource> branches)
    {
        foreach (BranchResource branch in branches)
        {
            if (person == null || branch == null || person.Stats.health <= 0f)
            {
                ClearGatherHighlights();
                gatherQueue = null;
                currentGatherPerson = null;
                yield break;
            }

            yield return GatherCommandService.Gather(person, branch);
            if (branch != null)
            {
                branch.SetGatherHighlighted(false);
                highlightedBranches.Remove(branch);
            }
        }

        ClearGatherHighlights();
        gatherQueue = null;
        currentGatherPerson = null;
    }

    private void ClearGatherHighlights()
    {
        foreach (BranchResource branch in highlightedBranches)
        {
            if (branch != null)
            {
                branch.SetGatherHighlighted(false);
            }
        }

        highlightedBranches.Clear();
    }

    private void CancelHighlightedGathers(PersonComponent person)
    {
        foreach (BranchResource branch in highlightedBranches)
        {
            if (branch != null)
            {
                branch.CancelGather(person);
                branch.SetGatherHighlighted(false);
            }
        }
    }

    private void CreateDragVisual()
    {
        GameObject dragObject = new GameObject("Gather Ground Drag Box");
        dragObject.transform.SetParent(transform, false);
        dragLine = dragObject.AddComponent<LineRenderer>();
        dragLine.useWorldSpace = true;
        dragLine.loop = false;
        dragLine.positionCount = 5;
        dragLine.startWidth = 0.08f;
        dragLine.endWidth = 0.08f;
        dragLine.material = new Material(Shader.Find("Sprites/Default"));
        dragLine.startColor = new Color(0.30f, 0.65f, 1f, 0.95f);
        dragLine.endColor = new Color(0.30f, 0.65f, 1f, 0.95f);
        dragObject.SetActive(false);
    }

    private void SetGroundDragRect(Vector3 start, Vector3 end)
    {
        const float lineHeight = 0.05f;
        float minX = Mathf.Min(start.x, end.x);
        float maxX = Mathf.Max(start.x, end.x);
        float minZ = Mathf.Min(start.z, end.z);
        float maxZ = Mathf.Max(start.z, end.z);

        Vector3 a = new(minX, lineHeight, minZ);
        Vector3 b = new(maxX, lineHeight, minZ);
        Vector3 c = new(maxX, lineHeight, maxZ);
        Vector3 d = new(minX, lineHeight, maxZ);

        dragLine.SetPosition(0, a);
        dragLine.SetPosition(1, b);
        dragLine.SetPosition(2, c);
        dragLine.SetPosition(3, d);
        dragLine.SetPosition(4, a);
    }

    private Bounds GetGroundSelectionBounds(Vector3 start, Vector3 end)
    {
        const float selectionHeight = 20f;
        float minX = Mathf.Min(start.x, end.x);
        float maxX = Mathf.Max(start.x, end.x);
        float minZ = Mathf.Min(start.z, end.z);
        float maxZ = Mathf.Max(start.z, end.z);
        Vector3 center = new((minX + maxX) * 0.5f, selectionHeight * 0.5f, (minZ + maxZ) * 0.5f);
        Vector3 size = new(Mathf.Max(0.2f, maxX - minX), selectionHeight, Mathf.Max(0.2f, maxZ - minZ));
        return new Bounds(center, size);
    }

    private bool TryGetGroundPoint(Vector2 screenPoint, out Vector3 groundPoint)
    {
        if (targetCamera == null)
        {
            targetCamera = Camera.main;
        }

        if (targetCamera == null)
        {
            groundPoint = Vector3.zero;
            return false;
        }

        Ray ray = targetCamera.ScreenPointToRay(screenPoint);
        RaycastHit[] hits = Physics.RaycastAll(ray, maxRayDistance);
        foreach (RaycastHit hit in hits.OrderBy(hit => hit.distance))
        {
            if (hit.collider.gameObject.name == "Ground")
            {
                groundPoint = hit.point;
                return true;
            }
        }

        Plane groundPlane = new(Vector3.up, Vector3.zero);
        if (groundPlane.Raycast(ray, out float distance))
        {
            groundPoint = ray.GetPoint(distance);
            return true;
        }

        groundPoint = Vector3.zero;
        return false;
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

    private static bool IsPointerOverUi()
    {
        return EventSystem.current != null && EventSystem.current.IsPointerOverGameObject();
    }
}
