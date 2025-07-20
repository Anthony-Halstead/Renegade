//  ItemDropSystem.cpp
#include "../CCL.h"
#include "ItemPickupComponents.h"
#include "GameComponents.h"
#include "WeaponComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../UTIL/Utilities.h"

#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace GAME;
using namespace GW::MATH;

/* ────────────────────────────── helpers ────────────────────────────── */
static PickupType WeightedPick(const ItemDropConfig& cfg)
{
    const float total = cfg.weights[0] + cfg.weights[1] + cfg.weights[2];
    float r = static_cast<float>(rand()) / RAND_MAX * total;

    if ((r -= cfg.weights[0]) < 0) return PickupType::Health;
    if ((r -= cfg.weights[1]) < 0) return PickupType::Shield;
    return PickupType::Ammo;
}

/* ─────────────────────────── config → manager ──────────────────────── */
void GAME::ConstructConfig(entt::registry& reg, entt::entity e)
{
    const auto& ini = *reg.ctx().get<UTIL::Config>().gameConfig;
    auto& cfg = reg.get<PickupManager>(e).itemDropConfig;

    cfg.dropChance = ini.at("ItemDrops").at("dropChance").as<float>();
    cfg.weights[0] = ini.at("ItemDrops").at("weightHealth").as<float>();
    cfg.weights[1] = ini.at("ItemDrops").at("weightShield").as<float>();
    cfg.weights[2] = ini.at("ItemDrops").at("weightWeapon").as<float>();
    cfg.fallSpeed = ini.at("ItemDrops").at("pickupFallSpeed").as<float>();
    cfg.despawnZ = ini.at("ItemDrops").at("despawnZ").as<float>();
}

/* ─────────────────────── runtime helper: spawn pickup ──────────────── */
void GAME::HandlePickup(entt::registry& reg, GVECTORF spawnPos)
{
    auto mgrView = reg.view<PickupManager>();
    if (mgrView.empty()) return;
    const auto& cfg = mgrView.get<PickupManager>(*mgrView.begin()).itemDropConfig;

    if (static_cast<float>(rand()) / RAND_MAX > cfg.dropChance) return;

    entt::entity p = reg.create();
    PickupType   kind = WeightedPick(cfg);

    reg.emplace<ItemPickup    >(p, ItemPickup{ kind });
    reg.emplace<PickupVelocity>(p, PickupVelocity{ -cfg.fallSpeed });

    float phase = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
    reg.emplace<PickupAnim>(p, PickupAnim{ spawnPos.y, 1.f, 0.f, phase });

    GMATRIXF M = GIdentityMatrixF;  M.row4 = spawnPos;
    reg.emplace<Transform>(p, Transform{ M });

    const char* key =
        (kind == PickupType::Health) ? "modelHealth" :
        (kind == PickupType::Shield) ? "modelShield" : "modelWeapon";

    UTIL::CreateDynamicObjects(reg, p,
        reg.ctx().get<UTIL::Config>().gameConfig->at("ItemDrops").at(key).as<std::string>());

    AUDIO::AudioSystem::PlaySFX("pickup", spawnPos);
}

/* ─────────── per-frame update: animate, despawn, collect, upgrade ──── */
void GAME::UpdatePickups(entt::registry& r, entt::entity /*unused*/)
{
    /* ----------------- manager & cfg ----------------- */
    auto mgrView = r.view<PickupManager>();
    if (mgrView.begin() == mgrView.end())
        return;
    const auto& cfg = mgrView.get<PickupManager>(*mgrView.begin()).itemDropConfig;

    const float dt = static_cast<float>(r.ctx().get<UTIL::DeltaTime>().dtSec);
    const float bobAmp = 0.5f, bobRate = 2.0f, collectR = 1.5f;

    /* ----------------- player ----------------- */
    auto pView = r.view<Player, Transform>();
    if (pView.begin() == pView.end())
        return;
    const auto playerEnt = *pView.begin();
    const auto playerPos = pView.get<Transform>(playerEnt).matrix.row4;

    /* ----------------- drops ----------------- */
    auto view = r.view<ItemPickup, PickupVelocity, PickupAnim, Transform>();
    for (auto e : view)
    {
        auto& vel = view.get<PickupVelocity>(e);
        auto& tf = view.get<Transform>(e);
        auto& anim = view.get<PickupAnim>(e);

        /* fall */
        tf.matrix.row4.z += vel.zPerSecond * dt;
        if (tf.matrix.row4.z < cfg.despawnZ) { r.destroy(e); continue; }

        /* collect */
        GVECTORF d;  GVector::SubtractVectorF(playerPos, tf.matrix.row4, d);
        float d2;    GVector::DotF(d, d, d2);
        if (d2 < collectR * collectR)
        {
            AUDIO::AudioSystem::PlaySFX("pickup", playerPos);

            /* if it was a weapon pickup, upgrade the gun */
            if (view.get<ItemPickup>(e).type == PickupType::Ammo)
                UpgradeWeapon(r, playerEnt);

            r.emplace_or_replace<Destroy>(e);
            continue;
        }

        /* spin + bob */
        anim.yaw += anim.spinRate * dt;
        if (anim.yaw > 6.28318f) anim.yaw -= 6.28318f;

        float bob = std::sinf(anim.bobPhase + bobRate * anim.yaw) * bobAmp;
        float newY = anim.baseY + bob;

        const float x = tf.matrix.row4.x, z = tf.matrix.row4.z;
        GMATRIXF rot; GMatrix::RotationYawPitchRollF(anim.yaw, 0.f, 0.f, rot);
        tf.matrix = rot;  tf.matrix.row4 = { x, newY, z, 1.f };
    }
}

/* ───────────────────────── ECS wiring ─────────────────────────────── */
CONNECT_COMPONENT_LOGIC()
{
    registry.on_construct<PickupManager>()
        .connect<&ConstructConfig>();

    registry.on_update<GameManager>()
        .connect<&UpdatePickups>();
}
