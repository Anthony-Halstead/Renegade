#include "WeaponComponents.h"

#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../CCL.h"

using namespace GAME;

/* -------------------------- construct cfg from INI ------------------------- */
void GAME::ConstructWeaponConfig(entt::registry& reg, entt::entity e)
{
    const auto& ini = *reg.ctx().get<UTIL::Config>().gameConfig;
    auto& cfg = reg.get<WeaponManager>(e).cfg;

    for (int i = 0; i < WeaponConfig::LEVELS; ++i)
    {
        std::string sec = "Weapon" + std::to_string(i);     // [Weapon0]..[Weapon3]
        cfg.fireRate[i] = ini.at(sec).at("rate").as<float>();
        cfg.meshName[i] = ini.at(sec).at("model").as<std::string>();
    }
}

/* --------------------------- upgrade helper -------------------------- */
void GAME::UpgradeWeapon(entt::registry& reg, entt::entity player)
{
    auto* pw = reg.try_get<PlayerWeapon>(player);
    if (!pw)                                     // player has no weapon? give one
        pw = &reg.emplace<PlayerWeapon>(player);

    if (pw->tier == WeaponTier::Max) return;     // already maxed out

    /* bump tier ------------------------------------------------------- */
    pw->tier = static_cast<WeaponTier>(static_cast<int>(pw->tier) + 1);

    /* grab config */
    auto mgrView = reg.view<WeaponManager>();
    if (mgrView.empty()) return;                 // should never happen
    const auto& cfg = mgrView.get<WeaponManager>(*mgrView.begin()).cfg;

    const int idx = static_cast<int>(pw->tier);
    pw->rate = cfg.fireRate[idx];
    pw->model = cfg.meshName[idx];

    if (pw->model.empty()) {
        std::cout << "[Weapon] Upgraded to tier " << idx
            << " (no mesh attached yet, rate=" << pw->rate << ")\n";
        return; 
    }

    /* swap the gun mesh (destroy old, spawn new) ---------------------- */
    if (auto* mc = reg.try_get<DRAW::MeshCollection>(player))
    {
        for (auto child : mc->entities)
            if (reg.valid(child)) reg.destroy(child);   // destroy child meshes

        mc->entities.clear();
    }

    UTIL::CreateDynamicObjects(reg, player, pw->model);
}
