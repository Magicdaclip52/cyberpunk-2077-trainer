#include "features/FeatureCatalog.h"

#include <Windows.h>

namespace er {
namespace {

void tune(std::vector<FeatureInfo>& k) {
    auto find = [&](const char* id) -> FeatureInfo* {
        for (auto& f : k) {
            if (f.id == id) {
                return &f;
            }
        }
        return nullptr;
    };
    auto mark = [&](const char* id, bool risky, float fv = 0, int iv = 0) {
        if (auto* f = find(id)) {
            f->risky = risky;
            f->needsInject = true;
            if (fv != 0) {
                f->defaultFloat = fv;
            }
            if (iv != 0) {
                f->defaultInt = iv;
            }
        }
    };
    mark("damage_multiplier", false, 2.0f);
    mark("defense_multiplier", false, 2.0f);
    mark("xp_multiplier", false, 2.0f);
    mark("streetcred_multiplier", false, 2.0f);
    mark("skill_xp_multiplier", false, 2.0f);
    mark("movement_speed", false, 2.0f);
    mark("game_speed", true, 2.0f);
    mark("edit_money", false, 0, 9999999);
    mark("edit_carry_weight", false, 0, 9999);
    mark("edit_attribute_points", false, 0, 50);
    mark("edit_perk_points", false, 0, 50);
    mark("edit_relic_points", false, 0, 20);
    mark("edit_player_level", true, 0, 60);
    mark("edit_streetcred_level", true, 0, 50);
    mark("edit_headhunter", true, 0, 60);
    mark("edit_netrunner", true, 0, 60);
    mark("edit_shinobi", true, 0, 60);
    mark("edit_solo", true, 0, 60);
    mark("edit_engineer", true, 0, 60);
    mark("fly_mode", true);
    mark("fly_height", true, 2.0f);
    mark("fly_speed", true, 2.0f);
    mark("one_hit_kill", true);
}

}  // namespace

const char* groupTitleRu(FeatureGroup group) {
    switch (group) {
        case FeatureGroup::Survival: return "Выживание";
        case FeatureGroup::Items:    return "Предметы";
        case FeatureGroup::Combat:   return "Бой";
        case FeatureGroup::World:    return "Мир / движение";
        case FeatureGroup::Stats:    return "Прогресс";
    }
    return "";
}

const char* groupTitleEn(FeatureGroup group) {
    switch (group) {
        case FeatureGroup::Survival: return "Survival";
        case FeatureGroup::Items:    return "Items";
        case FeatureGroup::Combat:   return "Combat";
        case FeatureGroup::World:    return "World / movement";
        case FeatureGroup::Stats:    return "Progress";
    }
    return "";
}

const char* hotkeySectionRu(bool ctrl, bool alt) {
    if (ctrl) {
        return "CTRL + NUM: прогресс";
    }
    if (alt) {
        return "ALT + NUM: предметы / мир";
    }
    return "NUM: выживание и бой";
}

const std::vector<FeatureInfo>& featureCatalog() {
    static std::vector<FeatureInfo> k = [] {
        std::vector<FeatureInfo> v = {
            {"infinite_health", "Бесконечное здоровье", "INFINITE HEALTH", FeatureGroup::Survival,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD1}},
            {"infinite_stamina", "Бесконечная выносливость", "INFINITE STAMINA", FeatureGroup::Survival,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD2}},
            {"stealth_mode", "Стелс (враги не видят)", "STEALTH MODE", FeatureGroup::Survival,
             FeatureKind::Toggle, {false, false, false, VK_SUBTRACT}},

            {"one_hit_kill", "Убийство с одного удара", "ONE HIT KILL", FeatureGroup::Combat,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD0}},
            {"damage_multiplier", "Множитель урона", "DAMAGE MULTIPLIER", FeatureGroup::Combat,
             FeatureKind::Toggle, {false, false, false, VK_DECIMAL}},
            {"defense_multiplier", "Множитель защиты", "DEFENSE MULTIPLIER", FeatureGroup::Combat,
             FeatureKind::Toggle, {false, false, false, VK_ADD}},
            {"no_reload", "Без перезарядки", "NO RELOAD", FeatureGroup::Combat, FeatureKind::Toggle,
             {false, false, false, VK_NUMPAD7}},
            {"super_accuracy", "Супер-точность", "SUPER ACCURACY", FeatureGroup::Combat,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD8}},
            {"no_recoil", "Без отдачи", "NO RECOIL", FeatureGroup::Combat, FeatureKind::Toggle,
             {false, false, false, VK_NUMPAD9}},
            {"grenade_no_cd", "Гранаты без отката", "GRENADES NO COOLDOWN", FeatureGroup::Combat,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD6}},
            {"pls_no_cd", "Пусковая система без отката", "PROJECTILE LAUNCH SYSTEM NO COOLDOWN",
             FeatureGroup::Combat, FeatureKind::Toggle, {false, false, false, VK_MULTIPLY}},
            {"heal_no_cd", "Лечение без отката", "HEALING ITEMS NO COOLDOWN", FeatureGroup::Combat,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD5}},

            {"infinite_items", "Бесконечные предметы/патроны", "INFINITE ITEMS/AMMO", FeatureGroup::Items,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD3}},
            {"items_no_decrease", "Предметы не тратятся", "ITEMS WON'T DECREASE", FeatureGroup::Items,
             FeatureKind::Toggle, {false, false, false, VK_NUMPAD4}},
            {"infinite_components", "Бесконечные компоненты", "INFINITE COMPONENTS", FeatureGroup::Items,
             FeatureKind::Toggle, {false, true, false, VK_NUMPAD3}},
            {"infinite_qh_components", "Бесконечные компоненты квикхаков", "INFINITE QUICKHACK COMPONENTS",
             FeatureGroup::Items, FeatureKind::Toggle, {false, true, false, VK_NUMPAD4}},
            {"edit_carry_weight", "Макс. переносимый вес", "EDIT MAX CARRYING WEIGHT", FeatureGroup::Items,
             FeatureKind::ApplyValue, {false, true, false, VK_NUMPAD5}},

            {"edit_money", "Изменить едди", "EDIT MONEY", FeatureGroup::Stats, FeatureKind::ApplyValue,
             {true, false, false, VK_NUMPAD1}},
            {"infinite_xp", "Бесконечный XP", "INFINITE XP", FeatureGroup::Stats, FeatureKind::Toggle,
             {true, false, false, VK_NUMPAD2}},
            {"xp_multiplier", "Множитель XP", "XP MULTIPLIER", FeatureGroup::Stats, FeatureKind::Toggle,
             {true, false, false, VK_NUMPAD3}},
            {"infinite_streetcred", "Бесконечный Street Cred", "INFINITE STREET CRED", FeatureGroup::Stats,
             FeatureKind::Toggle, {true, false, false, VK_NUMPAD4}},
            {"streetcred_multiplier", "Множитель Street Cred", "STREET CRED MULTIPLIER", FeatureGroup::Stats,
             FeatureKind::Toggle, {true, false, false, VK_NUMPAD5}},
            {"max_skill_xp", "Макс. XP навыков", "MAX SKILL XP/PROGRESSION", FeatureGroup::Stats,
             FeatureKind::Toggle, {true, false, false, VK_NUMPAD6}},
            {"skill_xp_multiplier", "Множитель XP навыков", "SKILL XP MULTIPLIER", FeatureGroup::Stats,
             FeatureKind::Toggle, {true, false, false, VK_NUMPAD7}},
            {"edit_attribute_points", "Очки атрибутов", "EDIT ATTRIBUTE POINTS", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {true, false, false, VK_NUMPAD8}},
            {"edit_perk_points", "Очки перков", "EDIT PERK POINTS", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {true, false, false, VK_NUMPAD9}},
            {"edit_relic_points", "Очки реликвий", "EDIT RELIC POINTS", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {true, false, false, VK_NUMPAD0}},
            {"edit_player_level", "Уровень персонажа", "EDIT PLAYER LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, true, false, VK_NUMPAD9}},
            {"edit_streetcred_level", "Уровень Street Cred", "EDIT STREET CRED LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, true, false, VK_NUMPAD0}},
            {"edit_headhunter", "Уровень Headhunter", "EDIT HEADHUNTER SKILL LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, false, true, VK_F1}},
            {"edit_netrunner", "Уровень Netrunner", "EDIT NETRUNNER SKILL LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, false, true, VK_F2}},
            {"edit_shinobi", "Уровень Shinobi", "EDIT SHINOBI SKILL LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, false, true, VK_F3}},
            {"edit_solo", "Уровень Solo", "EDIT SOLO SKILL LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, false, true, VK_F4}},
            {"edit_engineer", "Уровень Engineer", "EDIT ENGINEER SKILL LEVEL", FeatureGroup::Stats,
             FeatureKind::ApplyValue, {false, false, true, VK_F5}},
            {"ignore_cyberware", "Игнор лимита кибернетики", "IGNORE CYBERWARE CAPACITY", FeatureGroup::Stats,
             FeatureKind::Toggle, {true, false, false, VK_DECIMAL}},

            {"infinite_ram", "Бесконечная RAM", "INFINITE RAM", FeatureGroup::World, FeatureKind::Toggle,
             {false, true, false, VK_NUMPAD1}},
            {"freeze_breach", "Заморозка таймера Breach", "FREEZE BREACH PROTOCOL TIMER", FeatureGroup::World,
             FeatureKind::Toggle, {false, true, false, VK_NUMPAD2}},
            {"freeze_daytime", "Заморозить время суток", "FREEZE DAYTIME", FeatureGroup::World,
             FeatureKind::Toggle, {false, true, false, VK_DECIMAL}},
            {"daytime_plus_1h", "Время суток +1 час", "DAYTIME +1 HOUR", FeatureGroup::World,
             FeatureKind::ApplyValue, {false, true, false, VK_ADD}},

            {"game_speed", "Скорость игры", "SET GAME SPEED", FeatureGroup::World, FeatureKind::Toggle,
             {true, false, false, VK_ADD}},
            {"movement_speed", "Скорость движения", "SET MOVEMENT SPEED", FeatureGroup::World,
             FeatureKind::Toggle, {false, true, false, VK_NUMPAD6}},
            {"super_jump", "Супер-прыжок", "SUPER JUMP", FeatureGroup::World, FeatureKind::Toggle,
             {false, true, false, VK_NUMPAD7}},
            {"infinite_double_jump", "Бесконечные двойные прыжки", "INFINITE DOUBLE JUMPS", FeatureGroup::World,
             FeatureKind::Toggle, {false, true, false, VK_NUMPAD8}},
            {"fly_mode", "Режим полёта (Shift+стрелки)", "ENABLE FLY MODE", FeatureGroup::World,
             FeatureKind::Toggle, {false, false, true, VK_PRIOR}},
            {"fly_height", "Высота полёта", "SET FLY HEIGHT", FeatureGroup::World, FeatureKind::Toggle,
             {false, false, true, VK_ADD}},
            {"fly_speed", "Скорость полёта", "SET FLY SPEED", FeatureGroup::World, FeatureKind::Toggle,
             {false, false, true, VK_SUBTRACT}},

            {"no_fall_damage", "Нет урона от падения", "NO FALL DAMAGE", FeatureGroup::Survival,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"infinite_oxygen", "Бесконечный кислород", "INFINITE OXYGEN", FeatureGroup::Survival,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"always_crit", "Всегда критический удар", "ALWAYS CRITICAL HIT", FeatureGroup::Combat,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"infinite_sandevistan", "Бесконечный Сандевистан", "INFINITE SANDEVISTAN", FeatureGroup::Combat,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"free_shopping", "Бесплатные покупки", "FREE SHOPPING", FeatureGroup::Items,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"instant_crafting", "Мгновенный крафт", "INSTANT CRAFTING", FeatureGroup::Items,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"no_wanted", "Без розыска NCPD", "NO NCPD WANTED", FeatureGroup::World, FeatureKind::Toggle,
             {}, 0, 0, false, false, true},
            {"infinite_vehicle_health", "Бессмертие транспорта", "INFINITE VEHICLE HEALTH",
             FeatureGroup::World, FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"unlock_all_vehicles", "Все транспортные средства", "UNLOCK ALL VEHICLES", FeatureGroup::World,
             FeatureKind::Toggle, {}, 0, 0, false, false, true},
            {"auto_lockpick", "Авто-взлом замков", "AUTO LOCKPICK", FeatureGroup::World, FeatureKind::Toggle,
             {}, 0, 0, false, false, true},
        };
        tune(v);
        return v;
    }();
    return k;
}

}  // namespace er
