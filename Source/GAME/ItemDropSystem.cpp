
#include "../CCL.h"
#include "ItemPickupComponents.h"
#include "GameComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../UTIL/Utilities.h"



namespace GAME
{
	GAME::PickupType WeightedPick(ItemDropConfig& c)
	{
		const float total = c.weights[0] + c.weights[1] + c.weights[2];
		float r = static_cast<float>(rand()) / RAND_MAX * total;

		if ((r -= c.weights[0]) < 0) return PickupType::Health;
		if ((r -= c.weights[1]) < 0) return PickupType::Shield;
		return PickupType::Ammo;
	}

	void ConstructConfig(entt::registry& registry, entt::entity entity) {
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		auto& mgr = registry.get<PickupManager>(entity);
		mgr.itemDropConfig.dropChance = (*config).at("ItemDrops").at("dropChance").as<float>();
		mgr.itemDropConfig.weights[0] = (*config).at("ItemDrops").at("weightHealth").as<float>();
		mgr.itemDropConfig.weights[1] = (*config).at("ItemDrops").at("weightShield").as<float>();
		mgr.itemDropConfig.weights[2] = (*config).at("ItemDrops").at("weightWeapon").as<float>();
		mgr.itemDropConfig.fallSpeed = (*config).at("ItemDrops").at("pickupFallSpeed").as<float>();
		mgr.itemDropConfig.despawnZ = (*config).at("ItemDrops").at("despawnZ").as<float>();
	}

	void HandlePickup(entt::registry& reg, GW::MATH::GVECTORF pos)
	{
		auto& view = reg.view<PickupManager>();
		auto& e = *view.begin();

		auto& mgr = reg.get<PickupManager>(e);
		float roll = static_cast<float>(rand()) / RAND_MAX;
		if (roll > mgr.itemDropConfig.dropChance) {
			std::cout << "  Roll exceeded dropChance → no pickup spawned\n";
			return;
		}

		PickupType picked = WeightedPick(mgr.itemDropConfig);
		entt::entity p = reg.create();

		reg.emplace<ItemPickup>(p, ItemPickup{ picked });
		// reg.emplace<PickupVelocity>(p, PickupVelocity{ -cfg.fallSpeed });
		reg.emplace<Transform>(p, Transform{ GW::MATH::GIdentityMatrixF });
		auto& tran = reg.get<Transform>(p);
		tran.matrix.row4 = pos;

		const char* key =
			(picked == PickupType::Health) ? "modelHealth" :
			(picked == PickupType::Shield) ? "modelShield" : "modelWeapon";

		std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;
		std::string mdl = (*config).at("ItemDrops").at(key).as<std::string>();
		UTIL::CreateDynamicObjects(reg, p, mdl);
		AUDIO::AudioSystem::PlaySFX("pickup", pos);

	}

	// ───────────────────────────── per-frame pickup update ──────────────────
	void UpdatePickups(entt::registry& r, entt::entity /*unused*/)
	{
		const double dt = r.ctx().get<UTIL::DeltaTime>().dtSec;
		//const float  despawnZ = ItemDropConfig::Get().despawnZ;
		//auto view = r.view<ItemPickup, PickupVelocity, Transform>();
		//for (auto e : view)
		//{
		//	auto& pos = view.get<Transform>(e).matrix.row4;
		//	pos.z += view.get<PickupVelocity>(e).zPerSecond *
		//		static_cast<float>(dt);

		//	if (pos.z < despawnZ)           // fell off-screen
		//		r.destroy(e);
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<PickupManager>().connect<ConstructConfig>();
		/*	registry.on_update<PickupManager>().connect<UpdatePickups>();*/
	};

} // namespace GAME
