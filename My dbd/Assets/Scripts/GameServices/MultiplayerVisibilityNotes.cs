public static class MultiplayerVisibilityNotes
{
    public const string SharedWorldState =
        "People, enemies, resources, world props, movement orders, gathering results, crafting results, inventory changes, health, combat, and building placement must be shared.";

    public const string LocalOnlyState =
        "Selection, search text, opened windows, window position/size, minimap visibility, blueprint filters, hover highlights, and camera zoom/pan stay local.";
}
