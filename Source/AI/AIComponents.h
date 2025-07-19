#include <vector>
#include "FormationPatterns.h"
#ifndef AI_COMPONENTS_H_
#define AI_COMPONENTS_H_

namespace AI
{
	enum class BossState
	{
		WaveSpawning,
		ArcAttack,
		SpinningDrones,
		MineDrones,
	};

	struct BossTwoStates
	{
		BossState state = BossState::WaveSpawning;
	};
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

	struct OrbLifetime
	{
		float timeRemaining = 6.f;
	};
	struct MoveTarget { GW::MATH::GVECTORF pos; };
	struct CircleTag {};
	struct TimeAtPosition { float timeAtPosition = 0.0f; };
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
	struct OrbAttackCooldown
	{
		float cooldown = 6.0f;
		float timer = 0.0f;
	};
	struct Kamikaze {};
	struct SpawnKamikazeEnemy { float spawnTimer = 0.0f; };
	struct RushTarget { GW::MATH::GVECTORF trg; };

	struct BossHalfHealth {};
	struct BossFanAttack
	{
		bool isArcAttacking = false;

		float attackCooldown = 5.0f;
		float attackTimer = 0.0f;

		float arcInterval = 0.5f;
		float arcTimer = 0.0f;

		int arcsPerAttack = 5;
		int arcsFired = 0;

		float arcAngleRad = G_PI_F * 100.0f / 180.0f;
		float arcOffsetAmount = G_PI_F / 24.0f;
		bool alternateArcOffsets = true;
		int bulletCount = 9;

		float arcIncrement = G_PI_F / 12.0f;
		float currentFanAngle = 0.0f;

		float bulletOffsetDist = 1.0f;

		float spinSpeed = 0.0f;
		float idleSpinSpeed = G_PI_F * 0.1f;
		float maxSpinSpeed = G_PI_F * 1.0f;
		float spinUpRate = G_PI_F * 2.0f;
		bool spinningUp = false;
		bool spinningDown = false;
		bool atMaxSpin = false;
	};
	struct SpinningDroneSettings {
		float spinSpeed;
		float bulletFireInterval;
	};
	struct SpinningDronesCooldown {
		float cooldown = 15.0f;
		float timer = 15.f;
	};
	struct FlockSpawnCooldown
	{
		float cooldown = 18.f;
		float timer = 0.f;
		bool  pendingFirstImmediate = true;
	};
	struct SpinningDrone {
		GW::MATH::GVECTORF targetPosition;
		bool reachedTarget = false;
		float bulletFireTimer = 0.0f;
	};
	struct MineDroneSettings
	{
		float detonationDelay = 3.f;
		float blastScale = 4.f;
		float blastGrowthRate = 8.f;
	};
	struct MineDroneVolleyCooldown
	{
		float cooldown = 12.f;
		float timer = 0.f;
	};
	struct MineDrone
	{
		GW::MATH::GVECTORF targetPos = GW::MATH::GIdentityVectorF;
		float timer = 5.f;
		bool reachedTarget = false;
	};
	void ArcBullets(
		entt::registry& registry,
		entt::entity entity,
		const GW::MATH::GMATRIXF& bossMatrix,
		int bulletCount,
		float arcAngleRad,
		float baseAngleRad = 0.0f,
		float offsetDist = 1.0f
	);

	void SpinningDronesBehavior(entt::registry& registry, entt::entity entity, float deltaTime);

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