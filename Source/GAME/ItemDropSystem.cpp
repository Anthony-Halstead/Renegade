//  ItemDropSystem.cpp
#include "../CCL.h"
#include "ItemPickupComponents.h"
#include "GameComponents.h"
#include "WeaponComponents.h"
#include "ShieldComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../UTIL/Utilities.h"

#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace GAME;
using namespace GW::MATH;

/* ─────────────────────────── helpers ─────────────────────────── */
static PickupType WeightedPick(const ItemDropConfig& cfg)
{
    const float total = cfg.weights[0] + cfg.weights[1] + cfg.weights[2];
    float r = static_cast<float>(rand()) / RAND_MAX * total;

    if ((r -= cfg.weights[0]) < 0) return PickupType::Health;
    if ((r -= cfg.weights[1]) < 0) return PickupType::Shield;
    return PickupType::Ammo;
}

/* ───────────── load [ItemDrops] → PickupManager once ──────────── */
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

/* ────────── called by enemy-death to put a pickup in the world ───────── */
void GAME::HandlePickup(entt::registry& reg, GVECTORF spawnPos)
{
    auto mgrView = reg.view<PickupManager>();
    if (mgrView.empty()) return;
    const auto& cfg = mgrView.get<PickupManager>(*mgrView.begin()).itemDropConfig;

    if (static_cast<float>(rand()) / RAND_MAX > cfg.dropChance)
        return;

    entt::entity p = reg.create();
    PickupType   typ = WeightedPick(cfg);

    reg.emplace<ItemPickup    >(p, ItemPickup{ typ });
    reg.emplace<PickupVelocity>(p, PickupVelocity{ -cfg.fallSpeed });

    float phase = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
    reg.emplace<PickupAnim>(p, PickupAnim{ spawnPos.y, 1.f, 0.f, phase });

    GMATRIXF M = GIdentityMatrixF;  M.row4 = spawnPos;
    reg.emplace<Transform>(p, Transform{ M });

    const char* key =
        (typ == PickupType::Health) ? "modelHealth" :
        (typ == PickupType::Shield) ? "modelShield" :
        "modelWeapon";

    UTIL::CreateDynamicObjects(
        reg, p,
        reg.ctx().get<UTIL::Config>().gameConfig->at("ItemDrops").at(key).as<std::string>());

    AUDIO::AudioSystem::PlaySFX("pickup", spawnPos);
}

/* ───────── per-frame: animate, despawn, collect, apply effects ───────── */
void GAME::UpdatePickups(entt::registry& r, entt::entity /*unused*/)
{
    /* cfg */
    auto mView = r.view<PickupManager>();
    if (mView.begin() == mView.end()) return;
    const auto& cfg = mView.get<PickupManager>(*mView.begin()).itemDropConfig;

    const float dt = static_cast<float>(r.ctx().get<UTIL::DeltaTime>().dtSec);
    const float bobAmp = 0.5f;
    const float bobRate = 2.0f;
    const float collectR = 1.5f;

    /* player */
    auto pView = r.view<Player, Transform>();
    if (pView.begin() == pView.end()) return;
    entt::entity player = *pView.begin();
    const GVECTORF     playerPos = pView.get<Transform>(player).matrix.row4;

    /* each drop */
    auto view = r.view<ItemPickup, PickupVelocity, PickupAnim, Transform>();
    for (auto e : view)
    {
        auto& vel = view.get<PickupVelocity>(e);
        auto& tf = view.get<Transform>(e);
        auto& anim = view.get<PickupAnim>(e);

        /* fall */
        tf.matrix.row4.z += vel.zPerSecond * dt;
        if (tf.matrix.row4.z < cfg.despawnZ) { r.destroy(e); continue; }

        /* check radius for pick-up */
        GVECTORF d;  GVector::SubtractVectorF(playerPos, tf.matrix.row4, d);
        float d2;    GVector::DotF(d, d, d2);
        if (d2 < collectR * collectR)
        {
            AUDIO::AudioSystem::PlaySFX("pickup", playerPos);

            switch (view.get<ItemPickup>(e).type)
            {
            case PickupType::Ammo:   UpgradeWeapon(r, player); break;
            case PickupType::Shield:
            {
                /* fetch config */
                auto sView = r.view<ShieldManager>();
                const auto& sCfg = sView.get<ShieldManager>(*sView.begin()).cfg;

                /* add / replace Shield on the player */
                Shield sh;
                sh.hitsLeft = sCfg.hits;           // ← same as before

                /* create the ring mesh, but don’t try to store a return value */
                if (!sCfg.mesh.empty())
                    UTIL::CreateDynamicObjects(r, player, sCfg.mesh);

                r.emplace_or_replace<Shield>(player, sh);
                break;
            }
			case PickupType::Health:
			{
				auto* hp = r.try_get<Health>(player);
				if (hp)
				{
					hp->health += 1;  // +1 health
					if (hp->health > hp->maxHealth) hp->health = hp->maxHealth;
				}
				else
				{
					r.emplace<Health>(player, Health{ 1, 1 }); // create new health
				}
				break;
			}

            default: break;
            }

            r.emplace_or_replace<Destroy>(e);
            continue;
        }

        /* spin + bob */
        anim.yaw += anim.spinRate * dt;
        if (anim.yaw > 6.28318f) anim.yaw -= 6.28318f;

        float bob = std::sinf(anim.bobPhase + bobRate * anim.yaw) * bobAmp;
        float newY = anim.baseY + bob;
        float x = tf.matrix.row4.x;
        float z = tf.matrix.row4.z;

        GMATRIXF rot;  GMatrix::RotationYawPitchRollF(anim.yaw, 0.f, 0.f, rot);
        tf.matrix = rot;
        tf.matrix.row4 = { x, newY, z, 1.f };
    }
}

/* ──────────────────────── ECS wiring ──────────────────────── */
CONNECT_COMPONENT_LOGIC()
{
    registry.on_construct<PickupManager>()
        .connect<&ConstructConfig>();

    /* run every frame via GameManager’s update */
    registry.on_update<GameManager>()
        .connect<&UpdatePickups>();
}
