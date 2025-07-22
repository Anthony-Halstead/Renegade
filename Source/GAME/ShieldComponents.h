#pragma once

namespace GAME
{
    struct ShieldConfig {
        unsigned hits = 2;
        std::string mesh = "";
    };
    struct ShieldManager { ShieldConfig cfg; };

    void ConstructShieldConfig(entt::registry&, entt::entity);
}
