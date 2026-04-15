using UnityEngine;

public class EnemyComponent : MonoBehaviour
{
    [Header("Identity")]
    [SerializeField] private string enemyId;
    [SerializeField] private string enemyName;

    [Header("Combat")]
    [SerializeField] private PersonStats stats = new(80f, 8f, 0f);

    public string EnemyId => enemyId;
    public string EnemyName => enemyName;
    public PersonStats Stats => stats;
    public float Health => stats.health;
    public float Strength => stats.strength;
    public float Stamina => stats.stamina;

    private void OnMouseDown()
    {
        if (EnemyStatusWindow.Instance != null)
        {
            EnemyStatusWindow.Instance.ShowEnemy(this);
        }
    }

    public void Initialize(string id, string displayName, float health, float strength, float stamina)
    {
        enemyId = id;
        enemyName = displayName;
        stats = new PersonStats(health, strength, stamina);
        gameObject.name = enemyName;
    }
}
