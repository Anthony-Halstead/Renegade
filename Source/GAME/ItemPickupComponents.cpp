#include "ItemPickupComponents.h"
#include <cassert>

using namespace GAME;

static ItemDropConfig g_cfg;    // defaults

// pull numbers from GameConfig.ini the first time Get() is called
ItemDropConfig& ItemDropConfig::Get()
{
    static bool first = true;
    if (first)
    {
        first = false;
        auto& ctx = entt::locator<entt::registry>::value().ctx();
        if (ctx.contains<UTIL::Config>())
        {
            const auto& ini = *ctx.get<UTIL::Config>().gameConfig;
            try {
                g_cfg.dropChance = ini.at("ItemDrops").at("dropChance").as<float>();
                g_cfg.weights[0] = ini.at("ItemDrops").at("weightHealth").as<float>();
                g_cfg.weights[1] = ini.at("ItemDrops").at("weightShield").as<float>();
                g_cfg.weights[2] = ini.at("ItemDrops").at("weightWeapon").as<float>();
                g_cfg.fallSpeed = ini.at("ItemDrops").at("pickupFallSpeed").as<float>();
                g_cfg.despawnZ = ini.at("ItemDrops").at("despawnZ").as<float>();
            }
            catch (...) { /* keep defaults on any missing key */ }
        }
    }
    return g_cfg;
}
