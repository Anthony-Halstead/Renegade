#include "AIComponents.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"

#ifndef SPAWN_HELPERS_H_
#define SPAWN_HELPERS_H_

namespace AI
{

	inline entt::entity MakeAnchor(entt::registry& R, const std::vector<GW::MATH::GVECTORF>& slots)
	{
		entt::entity a = R.create();
		R.emplace<FormationAnchor>(a, FormationAnchor{ GW::MATH::GIdentityVectorF, slots });
		return a;
	}


	inline void SpawnMember(entt::registry& R, entt::entity anchor, uint16_t idx, const GW::MATH::GVECTORF& spawnPos)
	{
		std::shared_ptr<const GameConfig> config = R.ctx().get<UTIL::Config>().gameConfig;

		entt::entity e = R.create();
		std::string eModel = (*config).at("Enemy1").at("model").as<std::string>();
		unsigned int eHealth = (*config).at("Enemy1").at("hitpoints").as<unsigned int>();
		R.emplace<AI::FormationMember>(e, AI::FormationMember{ anchor, idx });
		R.emplace<AI::MoveTarget>(e);
		R.emplace<GAME::Enemy>(e);
		R.emplace<GAME::Health>(e, eHealth);
		GW::MATH::GMATRIXF enemyMatrix = GW::MATH::GIdentityMatrixF;
		enemyMatrix.row4 = spawnPos;
		R.emplace<GAME::Transform>(e, enemyMatrix);
		R.emplace<GAME::Velocity>(e);

		UTIL::CreateDynamicObjects(R, e, eModel);

	}


	inline void SpawnWave(entt::registry& R, FormationType kind, uint32_t spawnCount, const GW::MATH::GVECTORF& spawnPos, const GW::MATH::GVECTORF& anchorPos, float spacing = 2.5f)
	{
		auto slots = MakeSlots(kind, spacing);
		if (slots.empty()) return;

		entt::entity anchor = R.create();
		R.emplace<FormationAnchor>(anchor, FormationAnchor{ anchorPos, std::move(slots) });
		if (kind == FormationType::Circle8 || kind == FormationType::Circle12)
			R.emplace<CircleTag>(anchor);
		const auto& slotList = R.get<FormationAnchor>(anchor).slots;
		uint32_t n = std::min<uint32_t>(spawnCount, static_cast<uint32_t>(slotList.size()));

		for (uint16_t i = 0; i < n; ++i)
		{
			SpawnMember(R, anchor, i, spawnPos);
		}
	}

	inline void SpawnBoss(entt::registry& R, const char* name)
	{
		entt::entity enemyBoss = R.create();
		R.emplace<GAME::Enemy_Boss>(enemyBoss);

		std::shared_ptr<const GameConfig> config = R.ctx().get<UTIL::Config>().gameConfig;
		std::string enemyBossModel = (*config).at(name).at("model").as<std::string>();
		unsigned bossHealth = (*config).at(name).at("hitpoints").as<unsigned>();
		GAME::Transform enemyTransform{};

		float enemyBossScale = (*config).at(name).at("scale").as<float>();

		GW::MATH::GMATRIXF identity;
		GW::MATH::GMatrix::IdentityF(identity);

		GW::MATH::GMATRIXF scaleMatrix;
		GW::MATH::GMatrix::IdentityF(scaleMatrix);
		scaleMatrix.row1.x *= enemyBossScale;
		scaleMatrix.row2.y *= enemyBossScale;
		scaleMatrix.row3.z *= enemyBossScale;
		scaleMatrix.row4.w = 1.0f;

		GW::MATH::GMatrix::MultiplyMatrixF(enemyTransform.matrix, scaleMatrix, enemyTransform.matrix);

		enemyTransform.matrix = scaleMatrix;
		enemyTransform.matrix.row4.z = 30.0f;
		R.emplace<GAME::Health>(enemyBoss, bossHealth);
		R.emplace<GAME::Transform>(enemyBoss, enemyTransform);
		R.emplace<GAME::SpawnEnemies>(enemyBoss, 5.0f);

		UTIL::CreateDynamicObjects(R, enemyBoss, enemyBossModel);
	}
}
#endif