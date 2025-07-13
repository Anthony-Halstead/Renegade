#include "ItemPickupComponents.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../CCL.h"
#include <iostream>

namespace GAME
{
    // ───────────────────────────────────────── helpers ──────────────────────
    static PickupType WeightedPick(const ItemDropConfig& c)
    {
        const float total = c.weights[0] + c.weights[1] + c.weights[2];
        float       r = static_cast<float>(rand()) / RAND_MAX * total;

        if ((r -= c.weights[0]) < 0) return PickupType::Health;
        if ((r -= c.weights[1]) < 0) return PickupType::Shield;
        return PickupType::Ammo;
    }

    // ─────────────────────────── spawn-on-death callback ────────────────────
    static void OnEnemyDestroyed(entt::registry& reg, entt::entity dead)
    {
        const ItemDropConfig& cfg = ItemDropConfig::Get();

        // 80 % chance of no drop
        if (static_cast<float>(rand()) / RAND_MAX > cfg.dropChance)
            return;

        // ── pick position BEFORE we create anything ───────────────
        const Transform* t = reg.try_get<Transform>(dead);
        GW::MATH::GMATRIXF world = t ? t->matrix : GW::MATH::GIdentityMatrixF;

        // ── create the pickup entity ───────────────────────────────
        entt::entity p = reg.create();
        reg.emplace<ItemPickup>(p);
        reg.emplace<PickupData>(p, PickupData{ WeightedPick(cfg) });
        reg.emplace<PickupVelocity>(p, PickupVelocity{ -cfg.fallSpeed });
        reg.emplace<Transform>(p, Transform{ world });

        // ── model name from [ItemDrops] ────────────────────────────
        const char* key = (reg.get<PickupData>(p).type == PickupType::Health) ? "modelHealth" :
            (reg.get<PickupData>(p).type == PickupType::Shield) ? "modelShield" : "modelWeapon";

        const auto& ini = *reg.ctx().get<UTIL::Config>().gameConfig;
        const std::string mdl = ini.at("ItemDrops").at(key).as<std::string>();

        UTIL::CreateDynamicObjects(reg, p, mdl);

        if (auto* coll = reg.try_get<DRAW::MeshCollection>(p);
            !coll || coll->entities.empty())
        {
            std::cerr << "[ItemDropSystem] WARNING: model \"" << mdl
                << "\" could not be loaded.  Check [ItemDrops] in defaults.ini\n";
        }

        AUDIO::AudioSystem::PlaySFX("pickup", world.row4);
    }


    // ───────────────────────────── per-frame pickup update ──────────────────
    static void UpdatePickups(entt::registry& r, entt::entity /*unused*/)
    {
        const double dt = r.ctx().get<UTIL::DeltaTime>().dtSec;
        const float  despawnZ = ItemDropConfig::Get().despawnZ;

        auto view = r.view<ItemPickup, PickupVelocity, Transform>();
        for (auto e : view)
        {
            auto& pos = view.get<Transform>(e).matrix.row4;
            pos.z += view.get<PickupVelocity>(e).zPerSecond *
                static_cast<float>(dt);

            if (pos.z < despawnZ)           // fell off-screen
                r.destroy(e);
        }
    }

    // ─────────────────────────────── ECS registration ───────────────────────
    CONNECT_COMPONENT_LOGIC()
    {
        registry.on_destroy<Enemy>()
            .connect<&OnEnemyDestroyed>();

        registry.on_update<GameManager>()
            .connect<&UpdatePickups>();
    }

} // namespace GAME
