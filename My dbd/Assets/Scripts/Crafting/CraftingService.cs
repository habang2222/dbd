using System;
using System.Collections.Generic;
using Dbd.Items;
using UnityEngine;

namespace Dbd.Crafting
{
    public class CraftingService
    {
        public bool CanCraft(RecipeData recipe, Inventory inventory, CraftContext context)
        {
            return GetCraftFailureReasons(recipe, inventory, context).Count == 0;
        }

        public IReadOnlyList<CraftFailReason> GetCraftFailureReasons(RecipeData recipe, Inventory inventory, CraftContext context)
        {
            List<CraftFailReason> reasons = new();

            if (recipe == null)
            {
                reasons.Add(CraftFailReason.MissingRecipe);
                return reasons;
            }

            if (inventory == null)
            {
                reasons.Add(CraftFailReason.MissingInventory);
                return reasons;
            }

            if (context != null && !context.IsRecipeUnlocked(recipe.RecipeId))
            {
                reasons.Add(CraftFailReason.Locked);
            }

            if (!string.IsNullOrWhiteSpace(recipe.RequiredStation)
                && (context == null || context.StationId != recipe.RequiredStation))
            {
                reasons.Add(CraftFailReason.WrongStation);
            }

            if (!inventory.CanRemoveItems(recipe.Inputs))
            {
                reasons.Add(CraftFailReason.MissingMaterials);
            }

            if (!inventory.CanExchangeItems(recipe.Inputs, recipe.Outputs))
            {
                reasons.Add(CraftFailReason.InventoryFull);
            }

            foreach (RecipeRequirement requirement in recipe.Requirements)
            {
                if (!IsRequirementMet(requirement, context))
                {
                    reasons.Add(CraftFailReason.RequirementNotMet);
                    break;
                }
            }

            return reasons;
        }

        public bool ExecuteInstantCraft(RecipeData recipe, Inventory inventory, CraftContext context)
        {
            if (!CanCraft(recipe, inventory, context))
            {
                return false;
            }

            inventory.RemoveItems(recipe.Inputs);
            return inventory.AddItems(recipe.Outputs);
        }

        public CraftJob BeginCraftJob(RecipeData recipe, Inventory inventory, CraftContext context, float currentTime)
        {
            if (!CanCraft(recipe, inventory, context))
            {
                return null;
            }

            inventory.RemoveItems(recipe.Inputs);

            return new CraftJob
            {
                JobId = Guid.NewGuid().ToString("N"),
                RecipeId = recipe.RecipeId,
                CrafterEntityId = context?.CrafterEntityId,
                StartTime = currentTime,
                EndTime = currentTime + recipe.CraftTime,
                Status = CraftJobStatus.Running
            };
        }

        public bool CompleteCraftJob(CraftJob job, RecipeData recipe, Inventory inventory, float currentTime)
        {
            if (job == null || recipe == null || inventory == null)
            {
                return false;
            }

            if (job.Status != CraftJobStatus.Running || currentTime < job.EndTime)
            {
                return false;
            }

            if (!inventory.CanAddItems(recipe.Outputs))
            {
                return false;
            }

            bool added = inventory.AddItems(recipe.Outputs);
            if (added)
            {
                job.Status = CraftJobStatus.Completed;
            }

            return added;
        }

        private static bool IsRequirementMet(RecipeRequirement requirement, CraftContext context)
        {
            if (requirement == null || string.IsNullOrWhiteSpace(requirement.Type))
            {
                return true;
            }

            if (context == null)
            {
                return false;
            }

            if (requirement.Type == "SkillLevel")
            {
                return context.GetSkillLevel(requirement.Key) >= requirement.Value;
            }

            return false;
        }
    }
}
