#include "../AI/AIComponents.h"
#include "GameComponents.h"
#include "../UTIL/Utilities.h"

#ifndef DAMAGE_TYPES_H_
#define DAMAGE_TYPES_H_

namespace Damage
{
	inline void Explosion(entt::registry& reg, entt::entity entity)
	{
		std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;
		// This function is called when an explosion occurs.
		// It can be used to apply damage to entities within a certain radius.
		// For example, you might want to iterate through all entities and apply damage
		// based on their distance from the explosion center.

		std::cout << "Explosion triggered by entity: " << int(entity) << std::endl;

		// Create damaging radius and apply damage to all entities within range
		entt::entity e = reg.create();
		GAME::Transform* transform = reg.try_get<GAME::Transform>(entity);
		if (!transform) return;
		GW::MATH::GVECTORF explosionCenter = transform->matrix.row4;
		const float explosionRadius = (*config).at("Explosion").at("explosionRadius").as<float>();
		std::string model = (*config).at("Explosion").at("model").as<std::string>();
		const unsigned int explosionDamage = 1;

		// Model not loading, hitting null reference

		//UTIL::CreateDynamicObjects(reg, e, model);
		auto view = reg.view<GAME::Health, GAME::Transform>();
		for (auto e : view)
		{
			if (e == entity) continue; // Don't damage self
			auto& health = view.get<GAME::Health>(e);
			auto& t = view.get<GAME::Transform>(e);
			float dist = UTIL::Distance(explosionCenter, t.matrix.row4);
			if (dist <= explosionRadius)
			{
				health.health = (health.health > explosionDamage) ? health.health - explosionDamage : 0;
				std::cout << "Entity " << int(e) << " took " << explosionDamage << " damage from explosion. Remaining health: " << health.health << std::endl;
				if (health.health == 0 && !reg.all_of<GAME::Destroy>(e))
				{
					reg.emplace<GAME::Destroy>(e);
					std::cout << "Entity " << int(e) << " destroyed by explosion." << std::endl;
				}
			}
		}
	}
}
#endif
