#include <vector>
#include "FormationPatterns.h"
#ifndef AI_COMPONENTS_H_
#define AI_COMPONENTS_H_

namespace AI
{

	struct AIDirector {};
	//TODO: Impliment animation/behavioral state machine
	struct Agent
	{
		float moveSpeed;
		float rotationSpeed;
		float stoppingDistance;
	};

	struct FlockMember {};

	struct BoidStats
	{
		float maxSpeed;
		float safeRadius;
		float alignmentStrength;
		float cohesionStrength;
		float separationStrength;
		float seekStrength;
		float flockRadius;
	};

	struct FlockGoal
	{
		GW::MATH::GVECTORF pos;
		float time;
	};

	struct FormationAnchor
	{
		GW::MATH::GVECTORF origin = GW::MATH::GIdentityVectorF;
		std::vector<GW::MATH::GVECTORF> slots;
	};

	struct FormationMember
	{
		entt::entity anchor{ entt::null };
		uint16_t index{ 0 };
	};


	struct MoveTarget { GW::MATH::GVECTORF pos; };
	struct CircleTag {};
	struct TimeAtPosition{ float timeAtPosition = 0.0f; };
	struct FlyOffScreen {};
	struct ReturningToPosition {};
	struct HoldFire { float holdTime = 0.0f; };
	struct ShotsFired { int count = 0; };
	struct Explosion {};
	struct ExplosionGrowth {
		GW::MATH::GVECTORF targetScale;
		float growthRate;
	};
	struct OrbAttack {};
	struct OrbGrowth {
		GW::MATH::GVECTORF targetScale;
		float growthRate;
	};
	struct Kamikaze {};
	struct SpawnKamikazeEnemy { float spawnTimer = 0.0f; };
	struct RushTarget { GW::MATH::GVECTORF trg; };
	struct FinalBossShield {};
	struct BossParts {
		entt::entity vulnerableTop;
		entt::entity invulnerableTop;
	};
	struct Visible {
		bool enabled = true;
	};;
}
#endif