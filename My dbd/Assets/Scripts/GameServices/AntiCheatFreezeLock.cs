using UnityEngine;
using UnityEngine.AI;
using UnityEngine.UI;

public class AntiCheatFreezeLock : MonoBehaviour
{
    private Vector3 lockedPosition;
    private float frozenUntil;
    private Canvas warningCanvas;
    private Text warningText;

    public bool IsFrozen => Time.time < frozenUntil;

    public void Freeze(float seconds, string reason)
    {
        lockedPosition = transform.position;
        frozenUntil = Mathf.Max(frozenUntil, Time.time + seconds);
        StopMovement();
        ShowWarning(reason);

        PersonComponent person = GetComponent<PersonComponent>();
        if (person != null)
        {
            person.SetUnitStatus("DON'T CHEAT", "3분 이동 잠금");
        }
    }

    private void Update()
    {
        if (!IsFrozen)
        {
            if (warningCanvas != null && warningCanvas.gameObject.activeSelf)
            {
                warningCanvas.gameObject.SetActive(false);
            }

            return;
        }

        StopMovement();
        transform.position = lockedPosition;
        if (warningText != null)
        {
            int remaining = Mathf.CeilToInt(frozenUntil - Time.time);
            warningText.text = "DON'T CHEAT\n" + remaining + "s";
        }
    }

    private void StopMovement()
    {
        PersonMover mover = GetComponent<PersonMover>();
        if (mover != null)
        {
            mover.StopByCommand();
        }

        NavMeshAgent agent = GetComponent<NavMeshAgent>();
        if (agent != null && agent.isOnNavMesh)
        {
            agent.isStopped = true;
            agent.ResetPath();
        }
    }

    private void ShowWarning(string reason)
    {
        EnsureWarningCanvas();
        warningCanvas.gameObject.SetActive(true);
        warningText.text = "DON'T CHEAT\n180s";
        Debug.LogWarning($"Anti-cheat freeze applied to {gameObject.name}: {reason}");
    }

    private void EnsureWarningCanvas()
    {
        if (warningCanvas != null)
        {
            return;
        }

        warningCanvas = new GameObject("Anti Cheat Warning Canvas").AddComponent<Canvas>();
        warningCanvas.transform.SetParent(transform, false);
        warningCanvas.renderMode = RenderMode.WorldSpace;
        warningCanvas.transform.localPosition = new Vector3(0f, 2.6f, 0f);
        warningCanvas.transform.localScale = Vector3.one * 0.01f;

        RectTransform canvasRect = warningCanvas.GetComponent<RectTransform>();
        canvasRect.sizeDelta = new Vector2(260f, 90f);

        GameObject textObject = new GameObject("DON'T CHEAT Text");
        textObject.transform.SetParent(warningCanvas.transform, false);
        RectTransform textRect = textObject.AddComponent<RectTransform>();
        textRect.anchorMin = Vector2.zero;
        textRect.anchorMax = Vector2.one;
        textRect.offsetMin = Vector2.zero;
        textRect.offsetMax = Vector2.zero;

        warningText = textObject.AddComponent<Text>();
        warningText.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        warningText.fontSize = 26;
        warningText.fontStyle = FontStyle.Bold;
        warningText.alignment = TextAnchor.MiddleCenter;
        warningText.color = Color.red;
        warningText.text = "DON'T CHEAT";
    }
}
