#pragma once
#include <string>
#include <array>

namespace GAME
{
    enum class WeaponTier : std::uint8_t { Base = 0, Tier1, Tier2, Tier3, Max };

    struct PlayerWeapon           // lives on the *player* entity
    {
        WeaponTier tier = WeaponTier::Base;
        float      rate = 0.25f;            // shots / sec
        std::string model = "BaseGun";      // name used by CreateDynamicObjects
    };

    struct WeaponConfig           // singleton – owned by a WeaponManager entity
    {
        static constexpr std::size_t LEVELS = 4;    // Base + 3 upgrades

        /* per-tier parameters ( index == static_cast<int>(WeaponTier) ) */
        std::array<float, LEVELS> fireRate;         // shots / sec
        std::array<std::string, LEVELS> meshName;   // model names
    };

    struct WeaponManager { WeaponConfig cfg; };

    /* helpers -------------------------------------------------------- */
    void ConstructWeaponConfig(entt::registry&, entt::entity);
    void UpgradeWeapon(entt::registry&, entt::entity player);
} // namespace GAME
