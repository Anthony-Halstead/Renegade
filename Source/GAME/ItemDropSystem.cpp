#include "../CCL.h"
#include "ItemPickupComponents.h"
#include "GameComponents.h"
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

/* ────────────────────────────── config ctor ────────────────────────── */
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

    /*  roll the chance  */
    if (static_cast<float>(rand()) / RAND_MAX > cfg.dropChance) return;

    /*  create entity  */
    entt::entity p = reg.create();
    PickupType    kind = WeightedPick(cfg);

    reg.emplace<ItemPickup     >(p, ItemPickup{ kind });
    reg.emplace<PickupVelocity >(p, PickupVelocity{ -cfg.fallSpeed });

    /*  animation state  */
    float phase = static_cast<float>(rand()) / RAND_MAX * 6.28318f; // 0-2π
    reg.emplace<PickupAnim>(p, PickupAnim{ spawnPos.y, 1.f, 0.f, phase });

    /*  transform  */
    GMATRIXF M = GIdentityMatrixF;
    M.row4 = spawnPos;
    reg.emplace<Transform>(p, Transform{ M });

    /*  choose mesh  */
    const char* key =
        (kind == PickupType::Health) ? "modelHealth" :
        (kind == PickupType::Shield) ? "modelShield" : "modelWeapon";
    const auto& ini = *reg.ctx().get<UTIL::Config>().gameConfig;
    UTIL::CreateDynamicObjects(reg, p, ini.at("ItemDrops").at(key).as<std::string>());

    AUDIO::AudioSystem::PlaySFX("pickup", spawnPos);
}

/* ───────────────────── per-frame update (+animation & collect) ─────── */
void GAME::UpdatePickups(entt::registry& r, entt::entity /*unused*/)
{
    /* ----- configuration & dt ---------------------------------------- */
    auto mgrView = r.view<PickupManager>();
    if (mgrView.begin() == mgrView.end()) return;
    const ItemDropConfig& cfg =
        mgrView.get<PickupManager>(*mgrView.begin()).itemDropConfig;

    const double dt = r.ctx().get<UTIL::DeltaTime>().dtSec;
    const float  f_dt = static_cast<float>(dt);

    /* animation parameters */
    const float bobAmp = 0.5f;   // vertical amplitude
    const float bobRate = 2.0f;   // Hz
    const float pickRadius = 1.5f; // player-pickup collect radius

    /* ----- locate the player (if any) -------------------------------- */
    bool     havePlayer = false;
    GVECTORF playerPos{};

    auto pView = r.view<Player, Transform>();
    if (pView.begin() != pView.end()) {
        havePlayer = true;
        playerPos = pView.get<Transform>(*pView.begin()).matrix.row4;
    }

    /* ----- iterate all pickups --------------------------------------- */
    auto view = r.view<ItemPickup, PickupVelocity, PickupAnim, Transform>();
    for (auto e : view)
    {
        auto& vel = view.get<PickupVelocity>(e);
        auto& tf = view.get<Transform>(e);
        auto& anim = view.get<PickupAnim>(e);

        /* fall straight down (-Z) */
        tf.matrix.row4.z += vel.zPerSecond * f_dt;
        if (tf.matrix.row4.z < cfg.despawnZ) { r.destroy(e); continue; }

        /* check for collection ---------------------------------------- */
        if (havePlayer) {
            GVECTORF delta;
            GVector::SubtractVectorF(playerPos, tf.matrix.row4, delta);
            float dist2; GVector::DotF(delta, delta, dist2);

            if (dist2 < pickRadius * pickRadius)
            {
                AUDIO::AudioSystem::PlaySFX("pickup", playerPos);
                r.emplace_or_replace<Destroy>(e);   // remove the pickup
                continue;                           // done with this entity
            }
        }

        /* spin + bob animation ---------------------------------------- */
        anim.yaw += anim.spinRate * f_dt;
        if (anim.yaw > 6.28318f) anim.yaw -= 6.28318f;   // wrap @ 2π

        float bob = std::sinf(anim.bobPhase + bobRate * anim.yaw) * bobAmp;
        float newY = anim.baseY + bob;

        const float posX = tf.matrix.row4.x;
        const float posZ = tf.matrix.row4.z;

        GMATRIXF rot;
        GMatrix::RotationYawPitchRollF(anim.yaw, 0.f, 0.f, rot);

        tf.matrix = rot;                       // keep only rotation
        tf.matrix.row4 = { posX, newY, posZ, 1.f };
    }
}


/* ───────────────────────── ECS wiring ─────────────────────────────── */
CONNECT_COMPONENT_LOGIC()
{
    registry.on_construct<PickupManager>()
        .connect<&ConstructConfig>();

    /* advance + collect every frame, piggy-back on GameManager’s update */
    registry.on_update<GameManager>()
        .connect<&UpdatePickups>();
}
