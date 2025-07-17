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

		// Create damaging radius and apply damage to all entities within range
		entt::entity explosion = reg.create();
		reg.emplace<AI::Explosion>(explosion);

		GAME::Transform* transform = reg.try_get<GAME::Transform>(entity);
		if (!transform) return;

		const float explosionRadius = (*config).at("Explosion").at("explosionRadius").as<float>();
		std::string model = (*config).at("Explosion").at("model").as<std::string>();
		float explosionGrowth = (*config).at("Explosion").at("explosionGrowth").as<float>();

		UTIL::CreateTransform(reg, explosion, reg.get<GAME::Transform>(entity).matrix);
		UTIL::CreateDynamicObjects(reg, explosion, model);

		GW::MATH::GVECTORF targetScale = { explosionRadius, explosionRadius, explosionRadius, 0 };
		reg.emplace<AI::ExplosionGrowth>(explosion, targetScale, explosionGrowth);
	}

	///***This is just an idea for a basic bullet damage design, not currently used***///

	//inline void Bullet(entt::registry& reg, entt::entity entity)
	//{
	//	std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;

	//	float bulletDamage = (*config).at("Bullet").at("damage").as<float>();
	//	// This function is called when a bullet hits an entity.
	//	// It can be used to apply damage to the hit entity.
	//	std::cout << "Bullet hit entity: " << int(entity) << std::endl;
	//	auto* health = reg.try_get<GAME::Health>(entity);
	//	if (health)
	//	{
	//		health->health = (health->health > bulletDamage) ? health->health - bulletDamage : 0;
	//		std::cout << "Entity " << int(entity) << " took " << bulletDamage << " damage from bullet. Remaining health: " << health->health << std::endl;
	//		if (health->health == 0 && !reg.all_of<GAME::Destroy>(entity))
	//		{
	//			reg.emplace<GAME::Destroy>(entity);
	//			std::cout << "Entity " << int(entity) << " destroyed by bullet." << std::endl;
	//		}
	//	}
	//}

	inline void Lazer(entt::registry& reg, entt::entity entity)
	{
		std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;

		entt::entity lazerEntity = reg.create();
		float lazerDamage = (*config).at("EnemyLazer").at("damage").as<float>();
		std::string lazerModel = (*config).at("EnemyLazer").at("model").as<std::string>();
		UTIL::CreateDynamicObjects(reg, lazerEntity, lazerModel);
	}

	inline void OrbAttack(entt::registry& reg, entt::entity entity)
	{
		std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;

		entt::entity orbEntity = reg.create();
		reg.emplace<AI::OrbAttack>(orbEntity);
		reg.emplace<GAME::Velocity>(orbEntity);
		reg.emplace<GAME::Collidable>(orbEntity);
		reg.emplace<GAME::Bounded>(orbEntity);
		reg.emplace<GAME::Transform>(orbEntity, GW::MATH::GIdentityMatrixF);
		float orbDamage = (*config).at("Orb").at("damage").as<float>();
		std::string orbModel = (*config).at("Orb").at("model").as<std::string>();

		GAME::Transform* orbTransform = reg.try_get<GAME::Transform>(entity);
		if (!orbTransform) return;

		const float orbRadius = (*config).at("Orb").at("orbRadius").as<float>();
		float orbGrowth = (*config).at("Orb").at("orbGrowth").as<float>();

		UTIL::CreateTransform(reg, orbEntity, reg.get<GAME::Transform>(entity).matrix);
		UTIL::CreateDynamicObjects(reg, orbEntity, orbModel);

		GW::MATH::GVECTORF targetScale = { orbRadius, orbRadius, orbRadius, 0 };
		reg.emplace<AI::ExplosionGrowth>(orbEntity, targetScale, orbGrowth);
	}
}
#endif
