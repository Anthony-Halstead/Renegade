#include "AIComponents.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include <random> 
#include <cmath>
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
		R.emplace<AI::TimeAtPosition>(e);

		UTIL::CreateDynamicObjects(R, e, eModel);

	}
	inline GW::MATH::GVECTORF RandomInsideDiscXZ(float radius = 1.f)
	{
		using namespace GW::MATH;
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(-1.f, 1.f);

		float x, z;
		do {
			x = dist(rng);
			z = dist(rng);
		} while (x * x + z * z > 1.f);

		return GVECTORF{ radius * x, 0.f, radius * z, 0.f };
	}
	inline void SpawnFlock(entt::registry& registry, unsigned count, const GW::MATH::GVECTORF& spawnPos)
	{
		using namespace AI;
		using namespace GW::MATH;

		entt::entity e = registry.create();
		FlockGoal goal;
		goal.pos = GVECTORF{ 0,0,0,0 };
		registry.emplace<FlockGoal>(e, goal);

		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		std::string eModel = (*config).at("Enemy1").at("model").as<std::string>();
		unsigned int eHealth = (*config).at("Enemy1").at("hitpoints").as<unsigned int>();

		float speed = (*config).at("FlockSettings").at("speed").as<float>();
		float safeRadius = (*config).at("FlockSettings").at("safeRadius").as<float>();
		float align = (*config).at("FlockSettings").at("alignmentStrength").as<float>();
		float coh = (*config).at("FlockSettings").at("cohesionStrength").as<float>();
		float sep = (*config).at("FlockSettings").at("separationStrength").as<float>();
		float seek = (*config).at("FlockSettings").at("seekStrength").as<float>();
		float flockRadius = (*config).at("FlockSettings").at("flockRadius").as<float>();
		float xVelocity = (*config).at("FlockSettings").at("startingVelocityX").as<float>();
		float yVelocity = (*config).at("FlockSettings").at("startingVelocityY").as<float>();
		float zVelocity = (*config).at("FlockSettings").at("startingVelocityZ").as<float>();
		float spawnRadius = (*config).at("FlockSettings").at("spawnRadius").as<float>();
		for (unsigned int i = 0; i < count; i++) {
			auto e = registry.create();

			registry.emplace<FlockMember>(e);
			registry.emplace<BoidStats>(e, BoidStats{
				speed,
				safeRadius,
				align,
				coh,
				sep,
				seek,
				flockRadius });

			registry.emplace<GAME::Enemy>(e);
			registry.emplace<GAME::Health>(e, eHealth);
			GVECTORF offset = RandomInsideDiscXZ(spawnRadius);

			GVECTORF posWithJitter;
			GVector::AddVectorF(spawnPos, offset, posWithJitter);

			GMATRIXF enemyMatrix = GIdentityMatrixF;
			enemyMatrix.row4 = posWithJitter;

			registry.emplace<GAME::Transform>(e, enemyMatrix);
			registry.emplace<GAME::Velocity>(e,
				GAME::Velocity{ GVECTORF{
				xVelocity,
				yVelocity,
				zVelocity
				} });
			registry.emplace<GAME::Bounded>(e);
			UTIL::CreateDynamicObjects(registry, e, eModel);
		}
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

		R.emplace<GAME::BossTitle>(enemyBoss, GAME::BossTitle{ name });

		std::shared_ptr<const GameConfig> config = R.ctx().get<UTIL::Config>().gameConfig;
		std::string enemyBossModel = (*config).at(name).at("model").as<std::string>();
		unsigned bossHealth = (*config).at(name).at("hitpoints").as<unsigned>();
		GAME::Transform enemyTransform{ GW::MATH::GIdentityMatrixF };
		float enemyBossScale = (*config).at(name).at("scale").as<float>();
		UTIL::Scale(enemyTransform, enemyBossScale);
		enemyTransform.matrix.row4.z = 30.0f;
		R.emplace<GAME::Health>(enemyBoss, bossHealth);
		R.emplace<GAME::Transform>(enemyBoss, enemyTransform);
		R.emplace<GAME::SpawnEnemies>(enemyBoss, 5.0f);
		R.emplace<GAME::PriorFrameData>(enemyBoss, GAME::PriorFrameData{ bossHealth });

		UTIL::CreateDynamicObjects(R, enemyBoss, enemyBossModel);
	}
}
#endif