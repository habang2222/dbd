using System.Collections.Generic;

namespace Dbd.Crafting
{
    public static class BlueprintSampleData
    {
        public static BlueprintDatabase CreateDatabase()
        {
            BlueprintDatabase database = UnityEngine.ScriptableObject.CreateInstance<BlueprintDatabase>();
            database.ReplaceAll(CreateItems(), CreateRecipes());
            database.EnsureIndex();
            return database;
        }

        private static IEnumerable<BlueprintItem> CreateItems()
        {
            List<BlueprintItem> items = new List<BlueprintItem>();

            AddSeries(items, "leaf", "나뭇잎", 6, BlueprintItemCategory.Material);
            AddSeries(items, "branch", "나무가지", 5, BlueprintItemCategory.Material);
            AddSeries(items, "wood", "나무", 6, BlueprintItemCategory.Material);
            AddSeries(items, "sand", "모래", 3, BlueprintItemCategory.Material);
            AddSeries(items, "stone", "돌", 5, BlueprintItemCategory.Material);
            AddSeries(items, "dirt", "흙", 4, BlueprintItemCategory.Material);
            AddSeries(items, "coal", "석탄", 3, BlueprintItemCategory.Material);
            AddSeries(items, "copper", "구리", 3, BlueprintItemCategory.Material);
            items.Add(new BlueprintItem("lead", "납", BlueprintItemCategory.Material));
            AddSeries(items, "tin", "주석", 3, BlueprintItemCategory.Material);
            AddSeries(items, "iron", "철", 3, BlueprintItemCategory.Material);
            AddSeries(items, "water", "물", 3, BlueprintItemCategory.Material);
            AddSeries(items, "flint", "부싯돌", 3, BlueprintItemCategory.Material);

            Add(items, BlueprintItemCategory.Intermediate,
                ("leaf_bundle", "잎 묶음"),
                ("camouflage_leaves", "위장 잎더미"),
                ("twig_bundle", "잔가지 묶음"),
                ("straight_pole", "곧은 장대"),
                ("wood_plank", "나무 판자"),
                ("thick_plank", "두꺼운 판자"),
                ("coal_dust", "숯가루"),
                ("stone_dust", "거친 석분"),
                ("sharp_stone", "날카로운 돌조각"),
                ("wet_clay", "젖은 진흙"),
                ("clay_brick", "다진 진흙벽돌"),
                ("glass_shard", "유리 조각"),
                ("copper_wire", "구리선"),
                ("bronze_ingot", "청동 덩어리"),
                ("lead_spike", "납 가시"),
                ("iron_nail", "철못"),
                ("iron_plate", "철판"),
                ("blade_blank", "날붙이 원형"),
                ("handle", "손잡이"),
                ("leaf_cord", "가죽 대체 끈"),
                ("sticky_clay", "접착 진흙"),
                ("ember", "불씨"),
                ("reinforced_stake", "강화 말뚝"),
                ("hinge", "경첩"),
                ("nail_board", "간이 못판"),
                ("hidden_wall", "은폐벽"),
                ("spike_bundle", "가시 묶음"),
                ("furnace_body", "화로 몸체"),
                ("workbench_top", "작업대 상판"),
                ("rope_substitute", "밧줄 대체품"));

            Add(items, BlueprintItemCategory.Tool,
                ("workbench", "제작대"), ("stone_axe", "돌도끼"), ("iron_axe", "철도끼"),
                ("pickaxe", "곡괭이"), ("shovel", "삽"), ("hammer", "망치"), ("saw", "톱"),
                ("water_bucket", "물통"), ("smelter_mold", "제련 틀"));
            Add(items, BlueprintItemCategory.Weapon,
                ("sword", "검"), ("spear", "창"), ("dagger", "단검"), ("stone_knife", "돌칼"),
                ("bronze_spear", "청동 창"));
            Add(items, BlueprintItemCategory.Building,
                ("furnace", "화로"), ("door", "문"), ("roof_panel", "지붕 패널"),
                ("wall_panel", "벽 패널"), ("clay_wall", "진흙벽"), ("floor_panel", "바닥 패널"),
                ("window", "창문"), ("iron_bars", "철창살"), ("reinforced_door", "보강문"),
                ("defense_base", "방어 거점"));
            Add(items, BlueprintItemCategory.Placeable,
                ("shield", "방패"), ("barricade", "바리게이트"), ("trap", "함정"),
                ("torch", "횃불"), ("campfire", "캠프파이어"), ("storage_box", "저장 상자"),
                ("reinforced_box", "강화 상자"), ("bedroll", "침낭"), ("watch_fence", "감시 울타리"),
                ("stake_trap", "말뚝 함정"), ("lead_spike_trap", "납가시 함정"),
                ("fire_trap", "불 함정"), ("noise_trap", "소음 함정"),
                ("work_shelf", "작업 선반"), ("bronze_shield", "청동 방패"),
                ("hidden_barricade", "은폐 바리게이트"));

            return items;
        }

        private static IEnumerable<BlueprintRecipe> CreateRecipes()
        {
            return new[]
            {
                R("r_leaf_bundle", "leaf_bundle", I("leaf_1", 20)),
                R("r_camouflage_leaves", "camouflage_leaves", I("leaf_1", 50), I("dirt_2", 10)),
                R("r_twig_bundle", "twig_bundle", I("branch_1", 15)),
                R("r_straight_pole", "straight_pole", I("branch_3", 8), I("water_1", 1)),
                R("r_wood_plank", "wood_plank", I("wood_1", 5)),
                R("r_thick_plank", "thick_plank", I("wood_4", 8), I("water_2", 1)),
                R("r_coal_dust", "coal_dust", I("coal_1", 2), I("stone_1", 1)),
                R("r_stone_dust", "stone_dust", I("stone_1", 4)),
                R("r_sharp_stone", "sharp_stone", I("stone_3", 3), I("flint_1", 1)),
                R("r_wet_clay", "wet_clay", I("dirt_1", 10), I("water_1", 2)),
                R("r_clay_brick", "clay_brick", I("wet_clay", 3), I("sand_1", 6)),
                R("r_glass_shard", "glass_shard", I("sand_1", 8), I("coal_1", 2)),
                R("r_copper_wire", "copper_wire", I("copper_1", 2)),
                R("r_bronze_ingot", "bronze_ingot", I("copper_1", 3), I("tin_1", 1), I("coal_1", 2)),
                R("r_lead_spike", "lead_spike", I("lead", 2), I("coal_dust", 1)),
                R("r_iron_nail", "iron_nail", I("iron_1", 1)),
                R("r_iron_plate", "iron_plate", I("iron_2", 3), I("coal_2", 2)),
                R("r_blade_blank", "blade_blank", I("iron_1", 2), I("flint_1", 2)),
                R("r_handle", "handle", I("branch_1", 6), I("leaf_1", 8)),
                R("r_leaf_cord", "leaf_cord", I("leaf_5", 15), I("water_1", 1)),
                R("r_sticky_clay", "sticky_clay", I("wet_clay", 2), I("coal_dust", 1)),
                R("r_ember", "ember", I("flint_1", 2), I("coal_1", 1), I("leaf_1", 5)),
                R("r_reinforced_stake", "reinforced_stake", I("straight_pole", 2), I("sharp_stone", 4)),
                R("r_hinge", "hinge", I("iron_nail", 3), I("copper_wire", 2)),
                R("r_nail_board", "nail_board", I("wood_plank", 2), I("iron_nail", 10)),
                R("r_hidden_wall", "hidden_wall", I("camouflage_leaves", 1), I("twig_bundle", 2), I("sticky_clay", 2)),
                R("r_spike_bundle", "spike_bundle", I("lead_spike", 6), I("leaf_cord", 1)),
                R("r_furnace_body", "furnace_body", I("clay_brick", 8), I("stone_1", 12)),
                R("r_workbench_top", "workbench_top", I("thick_plank", 2), I("wood_plank", 4)),
                R("r_rope_substitute", "rope_substitute", I("leaf_cord", 3), I("leaf_1", 10)),
                R("r_workbench", "workbench", I("workbench_top", 1), I("wood_plank", 6), I("iron_nail", 8)),
                R("r_furnace", "furnace", I("furnace_body", 1), I("ember", 1), I("coal_dust", 5), I("iron_plate", 1)),
                R("r_sword", "sword", I("blade_blank", 2), I("handle", 1), I("iron_nail", 2), I("coal_dust", 2)),
                R("r_spear", "spear", I("straight_pole", 2), I("blade_blank", 1), I("leaf_cord", 2)),
                R("r_shield", "shield", I("thick_plank", 2), I("iron_plate", 1), I("leaf_cord", 2), I("iron_nail", 6)),
                R("r_barricade", "barricade", I("thick_plank", 4), I("nail_board", 2), I("reinforced_stake", 2)),
                R("r_door", "door", I("thick_plank", 3), I("hinge", 2), I("iron_nail", 12), I("handle", 1)),
                R("r_trap", "trap", I("hidden_wall", 1), I("nail_board", 1), I("spike_bundle", 2), I("rope_substitute", 1)),
                R("r_stone_axe", "stone_axe", I("handle", 1), I("sharp_stone", 3), I("leaf_cord", 1)),
                R("r_iron_axe", "iron_axe", I("handle", 1), I("blade_blank", 2), I("iron_nail", 4)),
                R("r_pickaxe", "pickaxe", I("straight_pole", 1), I("iron_plate", 1), I("blade_blank", 1), I("leaf_cord", 1)),
                R("r_shovel", "shovel", I("handle", 1), I("iron_plate", 1), I("wood_plank", 1)),
                R("r_hammer", "hammer", I("handle", 1), I("stone_5", 3), I("iron_nail", 2)),
                R("r_saw", "saw", I("blade_blank", 2), I("handle", 1), I("copper_wire", 2)),
                R("r_water_bucket", "water_bucket", I("wood_plank", 4), I("iron_plate", 1), I("sticky_clay", 2)),
                R("r_torch", "torch", I("branch_1", 3), I("leaf_1", 10), I("ember", 1), I("coal_dust", 1)),
                R("r_campfire", "campfire", I("branch_1", 12), I("wood_1", 4), I("ember", 1), I("stone_1", 8)),
                R("r_storage_box", "storage_box", I("wood_plank", 8), I("iron_nail", 10), I("hinge", 1)),
                R("r_reinforced_box", "reinforced_box", I("storage_box", 1), I("iron_plate", 2), I("hinge", 1)),
                R("r_bedroll", "bedroll", I("leaf_1", 60), I("leaf_cord", 4), I("twig_bundle", 1)),
                R("r_roof_panel", "roof_panel", I("leaf_1", 40), I("wood_plank", 3), I("leaf_cord", 2)),
                R("r_wall_panel", "wall_panel", I("wood_plank", 6), I("sticky_clay", 2), I("iron_nail", 6)),
                R("r_clay_wall", "clay_wall", I("clay_brick", 10), I("sticky_clay", 3)),
                R("r_floor_panel", "floor_panel", I("thick_plank", 3), I("wood_plank", 3), I("iron_nail", 8)),
                R("r_window", "window", I("glass_shard", 6), I("wood_plank", 2), I("sticky_clay", 1)),
                R("r_watch_fence", "watch_fence", I("reinforced_stake", 4), I("rope_substitute", 2), I("iron_nail", 8)),
                R("r_stake_trap", "stake_trap", I("reinforced_stake", 6), I("hidden_wall", 1), I("rope_substitute", 1)),
                R("r_lead_spike_trap", "lead_spike_trap", I("spike_bundle", 3), I("hidden_wall", 1), I("nail_board", 1)),
                R("r_fire_trap", "fire_trap", I("hidden_wall", 1), I("ember", 2), I("coal_dust", 8), I("leaf_1", 30)),
                R("r_noise_trap", "noise_trap", I("copper_wire", 4), I("glass_shard", 4), I("rope_substitute", 2), I("twig_bundle", 1)),
                R("r_dagger", "dagger", I("blade_blank", 1), I("handle", 1), I("leaf_cord", 1)),
                R("r_stone_knife", "stone_knife", I("sharp_stone", 2), I("handle", 1), I("leaf_1", 5)),
                R("r_iron_bars", "iron_bars", I("iron_plate", 3), I("iron_nail", 12), I("copper_wire", 2)),
                R("r_work_shelf", "work_shelf", I("workbench", 1), I("wood_plank", 8), I("iron_nail", 6)),
                R("r_smelter_mold", "smelter_mold", I("clay_brick", 6), I("sand_1", 10), I("iron_plate", 1)),
                R("r_bronze_shield", "bronze_shield", I("shield", 1), I("bronze_ingot", 3), I("iron_nail", 6)),
                R("r_bronze_spear", "bronze_spear", I("spear", 1), I("bronze_ingot", 2), I("leaf_cord", 1)),
                R("r_reinforced_door", "reinforced_door", I("door", 1), I("iron_plate", 3), I("hinge", 2), I("iron_nail", 14)),
                R("r_hidden_barricade", "hidden_barricade", I("barricade", 1), I("camouflage_leaves", 2), I("sticky_clay", 2)),
                R("r_defense_base", "defense_base", I("barricade", 2), I("reinforced_door", 1), I("watch_fence", 2), I("stake_trap", 2))
            };
        }

        private static void AddSeries(List<BlueprintItem> items, string idPrefix, string namePrefix, int count, BlueprintItemCategory category)
        {
            for (int index = 1; index <= count; index++)
            {
                items.Add(new BlueprintItem(idPrefix + "_" + index, namePrefix + " " + index, category));
            }
        }

        private static void Add(List<BlueprintItem> items, BlueprintItemCategory category, params (string id, string name)[] nextItems)
        {
            foreach ((string id, string name) in nextItems)
            {
                items.Add(new BlueprintItem(id, name, category));
            }
        }

        private static BlueprintIngredient I(string itemId, int amount)
        {
            return new BlueprintIngredient(itemId, amount);
        }

        private static BlueprintRecipe R(string id, string resultItemId, params BlueprintIngredient[] ingredients)
        {
            return new BlueprintRecipe(id, resultItemId, 1, ingredients);
        }
    }
}
