#ifndef ITEM_PICKUP_COMPONENTS_H_
#define ITEM_PICKUP_COMPONENTS_H_

namespace GAME
{
	struct ItemDropConfig
	{
		float  dropChance;                      // Chance of a drop spawning
		float  weights[3];    // Chance of each type Health, Shield, Weapon
		float  fallSpeed;                        // units / sec (-Z) speed of the drop
		float  despawnZ;                        // kill the drop when below this
	};
	enum class PickupType { Health, Shield, Ammo };
	// tags & data
	struct PickupManager { ItemDropConfig itemDropConfig; };
	struct ItemPickup { PickupType type; };                               // marks a drop in the world

	struct PickupVelocity { float zPerSecond; };
	void ConstructConfig(entt::registry& registry, entt::entity entity);
	void UpdatePickups(entt::registry& r, entt::entity /*unused*/);
	PickupType WeightedPick(ItemDropConfig& c);
	void HandlePickup(entt::registry& reg, GW::MATH::GVECTORF pos);
}
#endif
