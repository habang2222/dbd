using System;
using System.Collections.Generic;

namespace Dbd.Crafting
{
    [Serializable]
    public class CraftContext
    {
        public string CrafterEntityId;
        public string StationId;
        public HashSet<string> UnlockedRecipeIds = new();
        public Dictionary<string, int> SkillLevels = new();

        public bool IsRecipeUnlocked(string recipeId)
        {
            return UnlockedRecipeIds == null || UnlockedRecipeIds.Count == 0 || UnlockedRecipeIds.Contains(recipeId);
        }

        public int GetSkillLevel(string skillId)
        {
            if (SkillLevels == null || string.IsNullOrWhiteSpace(skillId))
            {
                return 0;
            }

            return SkillLevels.TryGetValue(skillId, out int level) ? level : 0;
        }
    }
}
