#include "ItemPickupComponents.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../CCL.h"

namespace GAME
{
    // helpers
    static PickupType WeightedPick(const ItemDropConfig& c)
    {
        const float total = c.weights[0] + c.weights[1] + c.weights[2];
        float r = static_cast<float>(rand()) / RAND_MAX * total;
        if ((r -= c.weights[0]) < 0) return PickupType::Health;
        if ((r -= c.weights[1]) < 0) return PickupType::Shield;
        return PickupType::Weapon;
    }

    // callback when an Enemy entity is destroyed
    static void OnEnemyDestroyed(entt::registry& reg, entt::entity dead)
    {
        const auto& cfg = ItemDropConfig::Get();

        // 80% of kills no drop
        if (static_cast<float>(rand()) / RAND_MAX > cfg.dropChance)
            return;

        // create new pickup entity
        entt::entity p = reg.create();
        reg.emplace<ItemPickup>(p);
        reg.emplace<PickupData>(p, PickupData{ WeightedPick(cfg) });
        reg.emplace<PickupVelocity>(p, PickupVelocity{ -cfg.fallSpeed });

        // copy world space transform from corpse
        const auto& corpseXform = reg.get<Transform>(dead).matrix;
        reg.emplace<Transform>(p, Transform{ corpseXform });

        // attach 3D model (using placeholders now)
        std::string modelName;
        switch (reg.get<PickupData>(p).type) {
        case PickupType::Health:  modelName = "assets/models/pickups/health.glb";  break;
        case PickupType::Shield:  modelName = "assets/models/pickups/shield.glb";  break;
        case PickupType::Weapon:  modelName = "assets/models/pickups/weapon.glb";  break;
        }
        UTIL::CreateDynamicObjects(reg, p, modelName);

        // spawn gentle loop SFX at the item’s position
        AUDIO::AudioSystem::PlaySFX("pickupSpawn", corpseXform.row4);
    }

    // register callback & per frame updater
    static void UpdatePickups(entt::registry& r, entt::entity)
    {
        const double dt = r.ctx().get<UTIL::DeltaTime>().dtSec;
        const float  despawnZ = ItemDropConfig::Get().despawnZ;

        auto view = r.view<ItemPickup, PickupVelocity, Transform>();
        for (auto e : view)
        {
            auto& t = view.get<Transform>(e).matrix.row4;
            const auto  vel = view.get<PickupVelocity>(e).zPerSecond;
            t.z += vel * static_cast<float>(dt);

            if (t.z < despawnZ)   // fell off screen
                r.destroy(e);
        }
    }

    // hook everything into the ECS
    CONNECT_COMPONENT_LOGIC()
    {
        // drop spawn when *any* Enemy component is removed because the entity is being destroyed
        registry.on_destroy<Enemy>().connect<&OnEnemyDestroyed>();

        // run UpdatePickups once per frame by piggy backing on the GameManager update
        registry.on_update<GameManager>().connect<&UpdatePickups>();
    }
}
