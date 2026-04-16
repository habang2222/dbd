using UnityEngine;

public sealed class PersonOwnerNameplate : MonoBehaviour
{
    private PersonComponent person;
    private TextMesh textMesh;

    private void Awake()
    {
        person = GetComponent<PersonComponent>();
        CreateLabel();
        Refresh();
    }

    private void LateUpdate()
    {
        if (textMesh == null)
        {
            return;
        }

        Camera camera = Camera.main;
        if (camera != null)
        {
            textMesh.transform.rotation = Quaternion.LookRotation(textMesh.transform.position - camera.transform.position);
        }
    }

    public void Refresh()
    {
        if (textMesh == null)
        {
            return;
        }

        if (person == null)
        {
            person = GetComponent<PersonComponent>();
        }

        textMesh.text = PlayerProfileService.GetUnitLabel(person);
    }

    private void CreateLabel()
    {
        GameObject labelObject = new GameObject("Owner Nameplate");
        labelObject.transform.SetParent(transform, false);
        labelObject.transform.localPosition = new Vector3(0f, 1.35f, 0f);

        textMesh = labelObject.AddComponent<TextMesh>();
        textMesh.anchor = TextAnchor.MiddleCenter;
        textMesh.alignment = TextAlignment.Center;
        textMesh.characterSize = 0.18f;
        textMesh.fontSize = 42;
        textMesh.color = Color.white;

        MeshRenderer renderer = labelObject.GetComponent<MeshRenderer>();
        if (renderer != null)
        {
            renderer.sortingOrder = 20;
        }
    }
}
