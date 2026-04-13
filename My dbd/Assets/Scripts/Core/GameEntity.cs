using Dbd.Items;
using UnityEngine;

namespace Dbd.Core
{
    public class GameEntity : MonoBehaviour
    {
        [SerializeField] private string entityId;
        [SerializeField] private string entityName;
        [SerializeField] private EntityStats stats = new();
        [SerializeField] private EntityState state;
        [SerializeField] private string currentAction;
        [SerializeField] private string teamId;
        [SerializeField] private ItemCatalog itemCatalog;
        [SerializeField] private int inventorySlotCount = 24;

        private Inventory inventory;

        public string EntityId => entityId;
        public string EntityName => entityName;
        public EntityStats Stats => stats;
        public EntityState State => state;
        public Vector3 Position => transform.position;
        public Inventory Inventory => inventory;
        public string CurrentAction => currentAction;
        public string TeamId => teamId;

        private void Awake()
        {
            if (string.IsNullOrWhiteSpace(entityId))
            {
                entityId = System.Guid.NewGuid().ToString("N");
            }

            inventory = new Inventory(inventorySlotCount, itemCatalog);
        }

        public void SetState(EntityState nextState, string nextAction)
        {
            state = nextState;
            currentAction = nextAction;
        }

        public void SetTeam(string nextTeamId)
        {
            teamId = nextTeamId;
        }
    }
}
