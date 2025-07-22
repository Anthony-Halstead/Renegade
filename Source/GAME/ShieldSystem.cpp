#include "ShieldComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"

using namespace GAME;

void GAME::ConstructShieldConfig(entt::registry& reg, entt::entity e)
{
    const auto& ini = *reg.ctx().get<UTIL::Config>().gameConfig;
    auto& cfg = reg.get<ShieldManager>(e).cfg;

    cfg.hits = ini.at("Shield").at("hits").as<unsigned>();
    cfg.mesh = ini.at("Shield").at("visualMesh").as<std::string>();
}

CONNECT_COMPONENT_LOGIC()
{
    registry.on_construct<ShieldManager>()
        .connect<&ConstructShieldConfig>();
}
