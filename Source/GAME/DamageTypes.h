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

	inline void EnemyLazerAttack(entt::registry& reg, entt::entity entity, GW::MATH::GVECTORF spawnPos)
	{
		std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;
		auto wView = reg.view<APP::Window>();
		if (wView.empty()) return;
		const auto& window = wView.get<APP::Window>(*wView.begin());

		float halfWidth = static_cast<float>(window.width) / 23.f;
		float halfHeight = static_cast<float>(window.height) / 23.f;

		float minX = -halfWidth;
		float maxX = halfWidth;
		float minZ = -halfHeight;
		float maxZ = halfHeight;

		GW::MATH::GVECTORF BR{ maxX,0.0f,minZ,0.0f };

		GW::MATH::GVECTORF brDir;
		GW::MATH::GVector::NormalizeF(BR, brDir);

		GW::MATH::GVECTORF blDir = brDir;
		blDir.z = -blDir.z;

		std::string lazerModel = (*config).at("EnemyLazer").at("model").as<std::string>();
		entt::entity lazerEntity = reg.create();

		GAME::Transform T{ GW::MATH::GIdentityMatrixF };
		GW::MATH::GVECTORF newPos = GW::MATH::GVECTORF{ spawnPos.x,spawnPos.y,spawnPos.z - 20.f,spawnPos.w };
		T.matrix.row4 = newPos;
		T.matrix.row3 = blDir;

		reg.emplace<GAME::Transform>(lazerEntity, T);
		reg.emplace<AI::LazerSweep>(lazerEntity, blDir, BR);

		UTIL::CreateDynamicObjects(reg, lazerEntity, lazerModel);

	}

	inline void EnemyOrbAttack(entt::registry& reg, const GW::MATH::GVECTORF& spawnPos)
	{
		std::shared_ptr<const GameConfig> config = reg.ctx().get<UTIL::Config>().gameConfig;

		entt::entity orbEntity = reg.create();

		reg.emplace<AI::OrbAttack>(orbEntity);
		const float orbRadius = (*config).at("Orb").at("orbRadius").as<float>();
		std::string orbModel = (*config).at("Orb").at("model").as<std::string>();
		float orbGrowth = (*config).at("Orb").at("orbGrowth").as<float>();

		GAME::Transform T{ GW::MATH::GIdentityMatrixF };
		T.matrix.row4 = spawnPos;
		reg.emplace<GAME::Transform>(orbEntity, T);

		UTIL::CreateDynamicObjects(reg, orbEntity, orbModel);

		GW::MATH::GVECTORF targetScale = { orbRadius, orbRadius, orbRadius, 0 };
		reg.emplace<AI::OrbGrowth>(orbEntity, targetScale, orbGrowth);
	}
}
#endif
