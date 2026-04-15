using System.Text;
using System.Collections.Generic;

public static class CraftingService
{
    public static bool CanCraft(PersonComponent crafter, Dbd.Crafting.BlueprintRecipe recipe)
    {
        if (!GameAuthority.CanIssueCommand(crafter) || recipe == null)
        {
            return false;
        }

        foreach (Dbd.Crafting.BlueprintIngredient ingredient in recipe.Ingredients)
        {
            if (!InventoryService.CanRemoveItem(crafter, ingredient.ItemId, ingredient.Amount))
            {
                return false;
            }
        }

        return true;
    }

    public static bool TryCraft(PersonComponent crafter, Dbd.Crafting.BlueprintItem item, Dbd.Crafting.BlueprintRecipe recipe, out string message)
    {
        if (AntiCheatService.IsFrozen(crafter))
        {
            message = "이 유닛은 부정 행동 감지로 제작할 수 없습니다.";
            return false;
        }

        if (!GameAuthority.CanIssueCommand(crafter))
        {
            message = "제작 권한이 없습니다.";
            if (crafter != null && !GameAuthority.IsOwnedByLocalClient(crafter))
            {
                AntiCheatService.Punish(crafter, GameAuthority.GetCommandRejectReason(crafter));
            }

            return false;
        }

        if (item == null || recipe == null)
        {
            message = "제작법을 찾을 수 없습니다.";
            AntiCheatService.Punish(crafter, "invalid crafting recipe");
            return false;
        }

        if (item.Id != recipe.ResultItemId)
        {
            message = "제작 결과가 제작법과 맞지 않습니다.";
            AntiCheatService.Punish(crafter, "craft result mismatch");
            return false;
        }

        if (!CanCraft(crafter, recipe))
        {
            message = "재료가 부족합니다.";
            return false;
        }

        if (!ServerItemGrantLedger.TryBeginGrant(crafter, recipe.ResultItemId, recipe.ResultAmount))
        {
            message = "제작 결과를 서버가 승인하지 못했습니다.";
            return false;
        }

        List<Dbd.Crafting.BlueprintIngredient> removedIngredients = new();
        foreach (Dbd.Crafting.BlueprintIngredient ingredient in recipe.Ingredients)
        {
            if (!InventoryService.RemoveItem(crafter, ingredient.ItemId, ingredient.Amount))
            {
                ServerItemGrantLedger.CancelGrant(crafter, recipe.ResultItemId, recipe.ResultAmount);
                ServerItemGrantLedger.EndGrant();
                RestoreIngredients(crafter, removedIngredients);
                message = "재료를 소모하는 중 실패했습니다.";
                return false;
            }

            removedIngredients.Add(ingredient);
        }

        bool resultAdded;
        try
        {
            resultAdded = InventoryService.AddItem(crafter, recipe.ResultItemId, recipe.ResultAmount);
        }
        finally
        {
            ServerItemGrantLedger.EndGrant();
        }

        if (!resultAdded)
        {
            RestoreIngredients(crafter, removedIngredients);
            message = "제작 결과 지급에 실패했습니다.";
            return false;
        }

        message = item.DisplayName + " 제작 완료";
        return true;
    }

    private static void RestoreIngredients(PersonComponent crafter, List<Dbd.Crafting.BlueprintIngredient> removedIngredients)
    {
        foreach (Dbd.Crafting.BlueprintIngredient ingredient in removedIngredients)
        {
            if (!ServerItemGrantLedger.TryBeginGrant(crafter, ingredient.ItemId, ingredient.Amount))
            {
                continue;
            }

            try
            {
                InventoryService.AddItem(crafter, ingredient.ItemId, ingredient.Amount);
            }
            finally
            {
                ServerItemGrantLedger.EndGrant();
            }
        }
    }

    public static string BuildRequirementText(Dbd.Crafting.BlueprintItem item, Dbd.Crafting.BlueprintRecipe recipe, PersonComponent crafter, string header)
    {
        StringBuilder builder = new();
        builder.AppendLine(item != null ? item.DisplayName : "알 수 없는 제작물");
        builder.AppendLine(header);
        builder.AppendLine();
        builder.AppendLine("필요 재료");

        if (recipe == null)
        {
            return builder.ToString();
        }

        foreach (Dbd.Crafting.BlueprintIngredient ingredient in recipe.Ingredients)
        {
            int owned = InventoryService.GetItemCount(crafter, ingredient.ItemId);
            builder.Append("- ");
            builder.Append(ingredient.ItemId);
            builder.Append(" ");
            builder.Append(owned);
            builder.Append("/");
            builder.AppendLine(ingredient.Amount.ToString());
        }

        return builder.ToString();
    }
}
