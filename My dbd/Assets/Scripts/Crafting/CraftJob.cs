using System;

namespace Dbd.Crafting
{
    [Serializable]
    public class CraftJob
    {
        public string JobId;
        public string RecipeId;
        public string CrafterEntityId;
        public float StartTime;
        public float EndTime;
        public CraftJobStatus Status;
    }
}
