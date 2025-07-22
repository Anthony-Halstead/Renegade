#include "AIComponents.h"
#include "SpawnHelpers.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"
#include "../APP/Window.hpp"
#include <random>
#include <algorithm>
#include "../GAME/DamageTypes.h"


namespace AI
{
	//FLOCK--------------------
	void ComputeFlockAverages(entt::registry& registry, GW::MATH::GVECTORF& outAvgPos, GW::MATH::GVECTORF& outAvgFwd)
	{
		auto view = registry.view<FlockMember, GAME::Transform, GAME::Velocity>();
		GW::MATH::GVECTORF sumPos{ 0,0,0,0 }, sumFwd{ 0,0,0,0 };
		size_t count = 0;

		for (auto e : view)
		{
			auto& M = registry.get<GAME::Transform>(e).matrix;
			auto& V = registry.get<GAME::Velocity>(e).vec;

			sumPos.x += M.row4.x;
			sumPos.y += M.row4.y;
			sumPos.z += M.row4.z;

			float v2; GW::MATH::GVector::DotF(V, V, v2);
			if (v2 > 1e-6f) {
				GW::MATH::GVECTORF norm;
				GW::MATH::GVector::NormalizeF(V, norm);
				sumFwd.x += norm.x;
				sumFwd.y += norm.y;
				sumFwd.z += norm.z;
			}
			++count;
		}

		if (count == 0) {
			outAvgPos = { 0,0,0,0 };
			outAvgFwd = { 0,0,0,0 };
		}
		else {
			float inv = 1.f / float(count);
			outAvgPos = { sumPos.x * inv,
						  sumPos.y * inv,
						  sumPos.z * inv,
						  0.f };
			outAvgFwd = { sumFwd.x * inv,
						  sumFwd.y * inv,
						  sumFwd.z * inv,
						  0.f };
		}
	}
	GW::MATH::GVECTORF CalculateAlignment(const GW::MATH::GVECTORF& avgFwd, const BoidStats& stats, float strength)
	{
		using namespace GW::MATH;
		GVECTORF vec = avgFwd;

		GVector::ScaleF(vec, 1.f / stats.maxSpeed, vec);
		float m2;
		GVector::DotF(vec, vec, m2);
		if (m2 > 1.0f)
			GVector::NormalizeF(vec, vec);
		GVector::ScaleF(vec, strength, vec);
		return vec;
	}
	GW::MATH::GVECTORF CalculateCohesion(const GW::MATH::GVECTORF& avgPos, const GW::MATH::GVECTORF& pos, float flockRadius, float strength)
	{
		using namespace GW::MATH;
		GVECTORF vec;
		GVector::SubtractVectorF(avgPos, pos, vec);
		float d = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
		GVector::NormalizeF(vec, vec);
		if (d < flockRadius) {
			GVector::ScaleF(vec, d / flockRadius, vec);
		}
		GVector::ScaleF(vec, strength, vec);
		return vec;
	}
	GW::MATH::GVECTORF CalculateSeparation(entt::registry& registry, entt::entity self, float safeRadius, float strength)
	{
		using namespace GW::MATH;
		GVECTORF sum{ 0,0,0,0 };
		auto& Tself = registry.get<GAME::Transform>(self).matrix;

		auto view = registry.view<FlockMember, AI::BoidStats, GAME::Transform>();
		for (auto other : view) {
			if (other == self) continue;
			auto& Tother = registry.get<GAME::Transform>(other).matrix;
			auto& statsO = registry.get<AI::BoidStats>(other);

			GVECTORF diff;
			GVector::SubtractVectorF(Tself.row4, Tother.row4, diff);
			float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
			float safe = safeRadius + statsO.safeRadius;

			if (dist > 0.f && dist < safe) {
				GVector::NormalizeF(diff, diff);
				GVector::ScaleF(diff, (safe - dist) / safe, diff);
				sum.x += diff.x;
				sum.y += diff.y;
				sum.z += diff.z;
			}
		}

		float m2; GVector::DotF(sum, sum, m2);
		if (m2 > 1.f)
			GVector::NormalizeF(sum, sum);
		GVector::ScaleF(sum, strength, sum);
		return sum;
	}
	GW::MATH::GVECTORF CalculateSeek(const GW::MATH::GVECTORF& pos, const GW::MATH::GVECTORF& goal, float maxSpeed, float strength)
	{
		using namespace GW::MATH;
		GVECTORF vec;
		GVector::SubtractVectorF(goal, pos, vec);

		float d2; GVector::DotF(vec, vec, d2);
		if (d2 < 1e-6f)
			return { 0,0,0,0 };

		GVector::NormalizeF(vec, vec);
		GVector::ScaleF(vec, strength, vec);
		return vec;
	}
	void ApplySteering(GAME::Velocity& vel, const GW::MATH::GVECTORF& steer, float maxSpeed, float dt)
	{
		using namespace GW::MATH;
		GVECTORF accel = steer;
		GVector::ScaleF(accel, maxSpeed * dt, accel);

		vel.vec.x += accel.x;
		vel.vec.y += accel.y;
		vel.vec.z += accel.z;

		float v2; GVector::DotF(vel.vec, vel.vec, v2);
		float ms2 = maxSpeed * maxSpeed;
		if (v2 > ms2) {
			GVector::NormalizeF(vel.vec, vel.vec);
			GVector::ScaleF(vel.vec, maxSpeed, vel.vec);
		}
	}
	void UpdateFlock(entt::registry& registry)
	{
		auto view = registry.view<FlockMember, GAME::Transform, GAME::Velocity, BoidStats>();

		float dt = registry.ctx().get<UTIL::DeltaTime>().dtSec;
		auto wView = registry.view<FlockGoal>();
		FlockGoal goal;
		if (!wView.empty())
			goal = wView.get<FlockGoal>(*wView.begin());

		GW::MATH::GVECTORF avgPos, avgFwd;
		ComputeFlockAverages(registry, avgPos, avgFwd);

		for (auto e : view) {
			auto& T = registry.get<GAME::Transform>(e);
			auto& M = T.matrix;
			auto& V = registry.get<GAME::Velocity>(e);
			auto& stats = registry.get<BoidStats>(e);

			auto align = CalculateAlignment(avgFwd, stats, stats.alignmentStrength);
			auto coh = CalculateCohesion(avgPos, M.row4, stats.flockRadius, stats.cohesionStrength);
			auto sep = CalculateSeparation(registry, e, stats.safeRadius, stats.separationStrength);
			auto seek = CalculateSeek(M.row4, goal.pos, stats.maxSpeed, stats.seekStrength);
			GW::MATH::GVECTORF steer = seek;
			steer.x += align.x + coh.x + sep.x;
			steer.y += align.y + coh.y + sep.y;
			steer.z += align.z + coh.z + sep.z;

			ApplySteering(V, steer, stats.maxSpeed, dt);
		}
	}
	void UpdateFlockGoal(entt::registry& registry)
	{
		using namespace GW::MATH;
		float dt = registry.ctx().get<UTIL::DeltaTime>().dtSec;
		auto& view = registry.view<FlockGoal>();
		if (view.empty()) return;
		auto& goal = view.get<FlockGoal>(*view.begin());
		goal.time -= dt;

		if (goal.time <= 0.f)
		{
			std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
			goal.pos = UTIL::RandomPointInWindowXZ(registry);
			float intervalSec = (*config).at("Timers").at("flockGoal").as<float>();
			goal.time = intervalSec;
		}
	}

	//HELPERS-----------------
	FormationType RandomFormationType()
	{
		static std::mt19937 rng{ std::random_device{}() };

		constexpr std::array<FormationType, 8> kinds = {
				FormationType::TriangleSmall,
				FormationType::TriangleLarge,
				FormationType::TwoRows8,
				FormationType::TwoRows12,
				FormationType::Circle8,
				FormationType::Circle12,
				FormationType::VSmall,
				FormationType::VLarge,
		};

		std::uniform_int_distribution<std::size_t> dist(0, kinds.size() - 1);
		return kinds[dist(rng)];
	}

	void ArcBullets(
		entt::registry& registry,
		entt::entity entity,
		const GW::MATH::GMATRIXF& matrix, int bulletCount, float arcAngleRad, float baseAngleRad, float offsetDist
	)
	{
		using namespace GW::MATH;
		const GVECTORF bossPos = matrix.row4;
		const auto& cfg = *registry.ctx().get<UTIL::Config>().gameConfig;
		const float speed = cfg.at("EnemyBullet").at("speed").as<float>();
		const std::string model = cfg.at("EnemyBullet").at("model").as<std::string>();
		float centerAngle = -G_PI_F / 2.0f;

		float startAngle = centerAngle - (arcAngleRad / 2.0f) + baseAngleRad;
		for (int i = 0; i < bulletCount; ++i) {
			float t = float(i) / float(bulletCount - 1);
			float angle = startAngle + t * arcAngleRad;

			GVECTORF dir = { std::cos(angle), 0.0f, std::sin(angle), 0.0f };

			GVECTORF spawnPos = bossPos;
			spawnPos.x += dir.x * offsetDist;
			spawnPos.z += dir.z * offsetDist;

			entt::entity b = registry.create();
			UTIL::CreateVelocity(registry, b, dir, speed);
			registry.emplace<GAME::BulletOwner>(b, entity);
			GMATRIXF bulletMatrix = matrix;
			bulletMatrix.row4 = spawnPos;
			UTIL::CreateTransform(registry, b, bulletMatrix);
			UTIL::CreateDynamicObjects(registry, b, model);
			registry.emplace<GAME::Bullet>(b);
		}
	}

	void ArcAttackBehavior(entt::registry& registry, entt::entity entity, AI::BossFanAttack& fan, float deltaTime)
	{
		if (fan.atMaxSpin) {
			fan.arcTimer -= deltaTime;
			if (fan.arcTimer <= 0.0f && fan.arcsFired < fan.arcsPerAttack) {
				float offset = 0.0f;
				if (fan.alternateArcOffsets) {
					if (fan.arcsFired % 2 == 1)
						offset = (fan.arcsFired % 4 == 1) ? -fan.arcOffsetAmount : +fan.arcOffsetAmount;
				}
				ArcBullets(
					registry,
					entity,
					registry.get<GAME::Transform>(entity).matrix,
					fan.bulletCount,
					fan.arcAngleRad,
					offset,
					fan.bulletOffsetDist
				);
				fan.arcTimer = fan.arcInterval;
				fan.arcsFired++;
			}
		}
	}
	void WaveSpawningBehavior(entt::registry& registry, entt::entity entity, float deltaTime)
	{
		auto& spawn = registry.get<GAME::SpawnEnemies>(entity);
		spawn.spawnTimer -= deltaTime;

		auto lesserEnemies = registry.view<GAME::Enemy>();
		if (spawn.spawnTimer <= 0.0f && lesserEnemies.empty())
		{
			spawn.spawnTimer = 20.0f;
			auto& bossTransform = registry.get<GAME::Transform>(entity);
			GW::MATH::GVECTORF bossPos = bossTransform.matrix.row4;
			SpawnWave(registry, AI::RandomFormationType(), 8, bossPos, GW::MATH::GVECTORF{ 0,-2,0,1 }, "Enemy2", 10);
		}
	}
	void MineBehavior(entt::registry& R, entt::entity boss, float dt)
	{
		if (!R.any_of<MineDroneVolleyCooldown>(boss))
			R.emplace<MineDroneVolleyCooldown>(boss);
		auto& cd = R.get<MineDroneVolleyCooldown>(boss);

		cd.timer -= dt;
		if (cd.timer > 0.f) return;

		cd.timer = cd.cooldown;

		const auto& cfg = *R.ctx().get<UTIL::Config>().gameConfig;
		const int  count = cfg.at("MineDrone").at("count").as<int>();

		const auto& bossT = R.get<GAME::Transform>(boss).matrix;
		for (int i = 0; i < count; ++i)
			SpawnMineDrone(R, bossT.row4);
	}
	void SpinningDronesBehavior(entt::registry& registry, entt::entity entity, float deltaTime)
	{
		auto dronesView = registry.view<AI::SpinningDrone>();

		if (dronesView.empty()) {
			const auto& cfg = *registry.ctx().get<UTIL::Config>().gameConfig;
			int droneCount = cfg.at("SpinningDrone").at("droneCount").as<int>();

			const auto& bossTransform = registry.get<GAME::Transform>(entity);
			GW::MATH::GVECTORF bossPos = bossTransform.matrix.row4;

			for (int i = 0; i < droneCount; ++i) {
				SpawnSpinningDrone(registry, bossPos);
			}
		}
	}
	void LazerSpawnBehavior(entt::registry& R, entt::entity boss, float dt)
	{
		if (!R.any_of<LazerCooldown>(boss))
			R.emplace_or_replace<LazerCooldown>(boss);
		auto& cd = R.get<LazerCooldown>(boss);

		cd.timer -= dt;
		if (cd.timer > 0.f) return;

		cd.timer = cd.cooldown;

		const GW::MATH::GVECTORF bossPos = R.get<GAME::Transform>(boss).matrix.row4;

		Damage::EnemyLazerAttack(R, boss, bossPos);
	}
	void KamikazeSpawnBehavior(entt::registry& R, entt::entity boss, float dt)
	{
		if (!R.any_of<SpawnKamikazeEnemy, GAME::Health>(boss))
			return;

		auto& hpComp = R.get<GAME::Health>(boss);
		if (hpComp.health > 2)
			return;

		auto& spawnK = R.get<SpawnKamikazeEnemy>(boss);
		spawnK.spawnTimer -= dt;
		if (spawnK.spawnTimer > 0.f)
			return;

		spawnK.spawnTimer = 3.0f;

		const GW::MATH::GVECTORF bossPos = R.get<GAME::Transform>(boss).matrix.row4;


		SpawnKamikaze(R, bossPos);
	}
	void FlockSpawnBehavior(entt::registry& R, entt::entity boss, float dt)
	{

		if (!R.any_of<FlockSpawnCooldown>(boss))
			R.emplace_or_replace<FlockSpawnCooldown>(boss);

		auto& flockCD = R.get<FlockSpawnCooldown>(boss);

		if (flockCD.pendingFirstImmediate) {
			flockCD.pendingFirstImmediate = false;
			flockCD.timer = flockCD.cooldown;
			const auto& bossPos = R.get<GAME::Transform>(boss).matrix.row4;
			SpawnFlock(R, 20, bossPos);
			return;
		}

		flockCD.timer -= dt;
		if (flockCD.timer > 0.f)
			return;

		flockCD.timer = flockCD.cooldown;

		const GW::MATH::GVECTORF bossPos = R.get<GAME::Transform>(boss).matrix.row4;
		SpawnFlock(R, 20, bossPos);
	}
	void UpdateMineDrones(entt::registry& R)
	{
		entt::basic_view players = R.view<GAME::Player, GAME::Transform>();
		if (players.begin() == players.end())return;
		auto& playerTransform = R.get<GAME::Transform>(*players.begin());
		GW::MATH::GVECTORF playerPos = playerTransform.matrix.row4;

		const float dt = static_cast<float>(R.ctx().get<UTIL::DeltaTime>().dtSec);
		const auto  cfg = *R.ctx().get<UTIL::Config>().gameConfig;

		auto view = R.view<GAME::Health, MineDrone, MineDroneSettings, GAME::Transform, GAME::Velocity>();
		for (auto e : view)
		{
			auto& d = view.get<MineDrone>(e);
			auto& set = view.get<MineDroneSettings>(e);
			auto& T = view.get<GAME::Transform>(e);
			auto& V = view.get<GAME::Velocity>(e);
			auto& hp = view.get<GAME::Health>(e);

			if (!d.reachedTarget)
			{
				const float maxSpd = cfg.at("MineDrone").at("speed").as<float>();
				d.reachedTarget = UTIL::SteerTowards(V, T.matrix.row4, d.targetPos, maxSpd);
				if (d.reachedTarget)
					d.timer = set.detonationDelay;
			}
			else
			{
				d.timer -= dt;
				if (hp.health <= 0)
				{
					Damage::Explosion(R, e);
					R.emplace_or_replace<GAME::Destroy>(e);
					R.remove<GAME::Enemy>(e);
					continue;
				}


				if (d.timer <= 0.f)
				{
					Damage::Explosion(R, e);
					R.emplace_or_replace<GAME::Destroy>(e);
				}
			}
		}
	}
	void UpdateSpinningDrones(entt::registry& registry) {
		auto view = registry.view<AI::SpinningDrone, AI::SpinningDroneSettings, GAME::Transform, GAME::Velocity>();
		float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		for (auto drone : view) {
			auto& droneData = view.get<AI::SpinningDrone>(drone);
			auto& droneSettings = view.get<AI::SpinningDroneSettings>(drone);
			auto& transform = view.get<GAME::Transform>(drone);
			auto& velocity = view.get<GAME::Velocity>(drone);

			if (!droneData.reachedTarget)
			{
				const float maxSpd = (*config).at("SpinningDrone").at("speed").as<float>();
				droneData.reachedTarget = UTIL::SteerTowards(velocity, transform.matrix.row4, droneData.targetPosition, maxSpd);
			}
			else {
				UTIL::RotateContinuous(transform, droneSettings.spinSpeed * deltaTime, 'Y');

				droneData.bulletFireTimer -= deltaTime;
				if (droneData.bulletFireTimer <= 0.0f) {
					droneData.bulletFireTimer = droneSettings.bulletFireInterval;

					const float bulletSpeed = (*config).at("EnemyBullet").at("speed").as<float>();
					const std::string bulletModel = (*config).at("EnemyBullet").at("model").as<std::string>();

					GW::MATH::GVECTORF fwd = transform.matrix.row3;
					fwd.y = 0;
					GW::MATH::GVector::NormalizeF(fwd, fwd);

					constexpr float muzzleOffset = 1.0f;
					GW::MATH::GVECTORF spawnPos = transform.matrix.row4;
					spawnPos.x += fwd.x * muzzleOffset;
					spawnPos.z += fwd.z * muzzleOffset;

					entt::entity bullet = registry.create();
					registry.emplace<GAME::BulletOwner>(bullet, drone);

					GAME::Transform bulletT{ GW::MATH::GIdentityMatrixF };
					bulletT.matrix.row4 = spawnPos;
					registry.emplace<GAME::Transform>(bullet, bulletT);

					GW::MATH::GVECTORF vel = { fwd.x * bulletSpeed, 0, fwd.z * bulletSpeed, 0 };
					registry.emplace<GAME::Velocity>(bullet, GAME::Velocity{ vel });

					UTIL::CreateDynamicObjects(registry, bullet, bulletModel);
					registry.emplace<GAME::Bullet>(bullet);

				}
			}
		}
	}
	void OrbSpawnBehavior(entt::registry& R, entt::entity boss, float dt)
	{
		if (!R.any_of<OrbAttackCooldown>(boss)) {
			R.emplace_or_replace<OrbAttackCooldown>(boss, OrbAttackCooldown{ 6.f, 5.f });
		}
		const auto& bossPos = R.get<GAME::Transform>(boss).matrix.row4;
		auto& cd = R.get<OrbAttackCooldown>(boss);
		cd.timer -= dt;
		if (cd.timer > 0.f)
			return;
		Damage::EnemyOrbAttack(R, bossPos);

		cd.timer = cd.cooldown;
	}
	void UpdateBossOneBehavior(entt::registry& R, entt::entity boss)
	{
		if (!R.valid(boss) || !R.all_of<GAME::Health, GAME::SpawnEnemies>(boss))
			return;

		auto& hp = R.get<GAME::Health>(boss);
		if (hp.health <= 0)
			return;
		const float dt = static_cast<float>(R.ctx().get<UTIL::DeltaTime>().dtSec);

		if (!R.any_of<GAME::SpawnEnemies>(boss))
			R.emplace_or_replace<GAME::SpawnEnemies>(boss);

		OrbSpawnBehavior(R, boss, dt);
		WaveSpawningBehavior(R, boss, dt);
		MineBehavior(R, boss, dt);
		KamikazeSpawnBehavior(R, boss, dt);
	}
	void UpdateBossTwoBehavior(entt::registry& registry, entt::entity entity)
	{
		if (!registry.valid(entity) ||
			!registry.all_of<GAME::Health, /*GAME::SpawnEnemies,*/ GAME::Transform>(entity))
			return;

		auto& hp = registry.get<GAME::Health>(entity);
		if (hp.health <= 0)
			return;
		if (!registry.any_of<AI::BossTwoStates>(entity))
			registry.emplace<AI::BossTwoStates>(entity);
		auto& bossState = registry.get<AI::BossTwoStates>(entity);

		bool newFan = false;
		if (!registry.any_of<AI::BossFanAttack>(entity)) {
			registry.emplace<AI::BossFanAttack>(entity);
			newFan = true;
		}
		auto& fan = registry.get<AI::BossFanAttack>(entity);

		if (!registry.any_of<AI::SpinningDronesCooldown>(entity))
			registry.emplace<AI::SpinningDronesCooldown>(entity,
				AI::SpinningDronesCooldown{ 15.f, 15.f });
		auto& droneCooldown = registry.get<AI::SpinningDronesCooldown>(entity);

		auto& spawn = registry.get<GAME::SpawnEnemies>(entity);

		const float dt = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);

		if (newFan || fan.attackTimer <= 0.f)
			fan.attackTimer = fan.attackCooldown;

		FlockSpawnBehavior(registry, entity, dt);

		if (bossState.state != AI::BossState::SpinningDrones) {
			droneCooldown.timer -= dt;
			if (droneCooldown.timer <= 0.f) {
				bossState.state = AI::BossState::SpinningDrones;
			}
		}

		static constexpr float MIN_WAVE_DWELL = 0.25f;
		static float waveDwellTimer = 0.f;

		switch (bossState.state)
		{
		case AI::BossState::WaveSpawning:
		{
			float preTimer = spawn.spawnTimer;

			//	WaveSpawningBehavior(registry, entity, dt); // UNCOMMENT ONCE TESTING IS DONE
				//---------Section TO REMOVE AFTER TESTING
			spawn.spawnTimer -= dt;
			if (spawn.spawnTimer <= 0.0f)
			{
				spawn.spawnTimer = 20.0f;
			}
			//---------Section TO REMOVE AFTER TESTING
			// Detect a wave spawn event (timer wrapped/reset)
			bool waveJustSpawned = (preTimer > 0.f && spawn.spawnTimer > preTimer);

			if (waveJustSpawned) {
				waveDwellTimer = 0.f;
			}
			else {
				waveDwellTimer += dt;
			}

			if (fan.spinSpeed > fan.idleSpinSpeed) {
				fan.spinSpeed -= fan.spinUpRate * dt;
				if (fan.spinSpeed < fan.idleSpinSpeed)
					fan.spinSpeed = fan.idleSpinSpeed;
			}
			else if (fan.spinSpeed < fan.idleSpinSpeed) {
				fan.spinSpeed += fan.spinUpRate * dt;
				if (fan.spinSpeed > fan.idleSpinSpeed)
					fan.spinSpeed = fan.idleSpinSpeed;
			}

			if (waveJustSpawned || waveDwellTimer >= MIN_WAVE_DWELL) {
				if (fan.attackTimer > 0.f)
					fan.attackTimer -= dt;
			}

			if (fan.attackTimer <= 0.f) {
				bossState.state = AI::BossState::ArcAttack;
				fan.arcTimer = 0.f;
				fan.arcsFired = 0;
				fan.spinningUp = true;
				fan.spinningDown = false;
				fan.atMaxSpin = false;
			}
			break;
		}

		case AI::BossState::ArcAttack:
		{
			if (fan.spinningUp) {
				fan.spinSpeed += fan.spinUpRate * dt;
				if (fan.spinSpeed >= fan.maxSpinSpeed) {
					fan.spinSpeed = fan.maxSpinSpeed;
					fan.spinningUp = false;
					fan.atMaxSpin = true;
				}
			}
			else if (fan.atMaxSpin) {
				if (fan.arcsFired >= fan.arcsPerAttack) {
					fan.atMaxSpin = false;
					fan.spinningDown = true;
				}
			}
			else if (fan.spinningDown) {
				fan.spinSpeed -= fan.spinUpRate * dt;
				if (fan.spinSpeed <= fan.idleSpinSpeed) {
					fan.spinSpeed = fan.idleSpinSpeed;
					fan.spinningDown = false;
					bossState.state = AI::BossState::WaveSpawning;
					fan.attackTimer = fan.attackCooldown;
					fan.arcTimer = 0.f;
					fan.arcsFired = 0;
					fan.currentFanAngle = 0.f;
				}
			}
			break;
		}

		case AI::BossState::SpinningDrones:
		{
			SpinningDronesBehavior(registry, entity, dt);
			droneCooldown.timer = droneCooldown.cooldown;

			if (fan.attackTimer <= 0.f)
				fan.attackTimer = fan.attackCooldown;

			bossState.state = AI::BossState::WaveSpawning;
			break;
		}

		default:
			break;
		}

		auto& bossTransform = registry.get<GAME::Transform>(entity);
		UTIL::RotateContinuous(bossTransform, dt * fan.spinSpeed, 'Y');

		if (bossState.state == AI::BossState::ArcAttack)
			ArcAttackBehavior(registry, entity, fan, dt);
	}
	void UpdateBossThreeBehavior(entt::registry& registry, entt::entity& entity)
	{
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		if (!registry.valid(entity) || !registry.all_of<GAME::Health, GAME::SpawnEnemies>(entity))
			return;
		const float dt = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);

		if (!registry.any_of<GAME::SpawnEnemies>(entity))
			registry.emplace_or_replace<GAME::SpawnEnemies>(entity);

		auto& hp = registry.get<GAME::Health>(entity);
		auto& spawn = registry.get<GAME::SpawnEnemies>(entity);

		if (hp.health <= 0)
			return;

		LazerSpawnBehavior(registry, entity, dt);
		OrbSpawnBehavior(registry, entity, dt);
		WaveSpawningBehavior(registry, entity, dt);
		MineBehavior(registry, entity, dt);
		FlockSpawnBehavior(registry, entity, dt);
		KamikazeSpawnBehavior(registry, entity, dt);
	}

	void Initialize(entt::registry& registry)
	{
		SpawnBoss(registry, "EnemyBoss_Station");
	}

	void UpdateFormation(entt::registry& r)
	{
		auto view = r.view<
			FormationMember,
			MoveTarget,
			GAME::Transform,
			GAME::Velocity>(entt::exclude<AI::FlyOffScreen, AI::ReturningToPosition>);

		const float rotSpeedDeg = 90.f;
		const float rotStepRad = G_DEGREE_TO_RADIAN_F(rotSpeedDeg) * r.ctx().get<UTIL::DeltaTime>().dtSec;

		for (auto e : view)
		{

			const auto& fm = view.get<FormationMember>(e);
			if (!r.valid(fm.anchor) || !r.all_of<FormationAnchor>(fm.anchor)) continue;
			const auto& anchor = r.get<FormationAnchor>(fm.anchor);
			if (fm.index > anchor.slots.size()) continue;

			const GW::MATH::GVECTORF slotPos = anchor.slots[fm.index];
			const GW::MATH::GVECTORF anchorPos = anchor.origin;
			view.get<MoveTarget>(e).pos = slotPos;

			const GW::MATH::GMATRIXF& m = view.get<GAME::Transform>(e).matrix;

			if (UTIL::Distance(m.row4, slotPos) > 50.0f) continue;

			if (!r.any_of<CircleTag>(fm.anchor)) continue;
			UTIL::RotateTowards(view.get<GAME::Transform>(e), anchorPos, rotStepRad);
		}
	}

	void UpdateLocomotion(entt::registry& r)
	{
		auto view = r.view<MoveTarget, GAME::Velocity, GAME::Transform>(entt::exclude<FlockMember>);
		double dt = r.ctx().get<UTIL::DeltaTime>().dtSec;
		float speed = (*r.ctx().get<UTIL::Config>().gameConfig).at("Enemy1").at("speed").as<float>();

		auto wView = r.view<APP::Window>();
		if (wView.empty()) return;
		const auto& window = wView.get<APP::Window>(*wView.begin());

		float halfWidth = static_cast<float>(window.width) / 23.0f;
		float halfHeight = static_cast<float>(window.height) / 23.0f;

		float minX = -halfWidth;
		float maxX = halfWidth;
		float minZ = -halfHeight;
		float maxZ = halfHeight;

		for (auto e : view)
		{
			auto& pos = view.get<GAME::Transform>(e).matrix.row4;

			// Enemy flies off screen
			if (r.any_of<AI::FlyOffScreen>(e))
			{
				float speed = (*r.ctx().get<UTIL::Config>().gameConfig).at("Enemy1").at("speed").as<float>();
				view.get<GAME::Velocity>(e).vec = { speed, 0, 0, 0 };

				// Wrap around screen edges
				if (pos.x < minX)
				{
					pos.x = maxX;
					r.remove<AI::FlyOffScreen>(e);
					r.emplace_or_replace<AI::ReturningToPosition>(e);
				}
				else if (pos.x > maxX)
				{
					pos.x = minX;
					r.remove<AI::FlyOffScreen>(e);
					r.emplace_or_replace<AI::ReturningToPosition>(e);
				}
				continue;
			}

			// Enemy is returning to position
			if (r.any_of<AI::ReturningToPosition>(e))
			{
				const auto target = view.get<MoveTarget>(e).pos;
				auto& vel = view.get<GAME::Velocity>(e).vec;

				if (UTIL::Distance(target, pos) < 0.1f)
				{
					vel = { 0,0,0,0 };
					r.remove<AI::ReturningToPosition>(e); // Enemy back in position
				}
				else
				{
					GW::MATH::GVECTORF delta;
					GW::MATH::GVector::SubtractVectorF(target, pos, delta);
					GW::MATH::GVector::NormalizeF(delta, delta);
					GW::MATH::GVector::ScaleF(delta, speed, vel);
				}
				continue;
			}

			const auto& trg = view.get<MoveTarget>(e).pos;
			auto& vel = view.get<GAME::Velocity>(e).vec;

			GW::MATH::GVECTORF delta;
			GW::MATH::GVector::SubtractVectorF(trg, pos, delta);

			if (UTIL::Distance(pos, trg) > 0.1f)
			{
				GW::MATH::GVECTORF dir;
				GW::MATH::GVector::NormalizeF(delta, dir);
				GW::MATH::GVector::ScaleF(dir, speed, vel);
			}
			else
				vel = { 0,0,0,0 };
		}
	}

	void EnemyFlyOffScreen(entt::registry& r)
	{
		// Check if any enemies are already flying off screen
		auto flyView = r.view<AI::FlyOffScreen>();
		auto returnView = r.view<AI::ReturningToPosition>();
		if (!flyView.empty() || !returnView.empty()) return;

		// Collect all enemies
		std::vector<entt::entity> enemies;
		auto view = r.view<GAME::Enemy, GAME::Transform>();
		for (auto e : view) enemies.push_back(e);

		if (enemies.empty()) return;

		// Randomly select one enemy
		static std::mt19937 rng{ std::random_device{}() };
		std::uniform_int_distribution<std::size_t> dist(0, enemies.size() - 1);
		entt::entity selectedEnemy = enemies[dist(rng)];

		// Tag to fly off screen
		if (!r.any_of<AI::FlyOffScreen>(selectedEnemy))
			r.emplace_or_replace<AI::FlyOffScreen>(selectedEnemy);
	}

	void EnemyInvulnerability(entt::registry& registry, const float& deltaTime)
	{
		auto view = registry.view<GAME::Enemy_Boss, GAME::Invulnerability>();
		for (auto ent : view)
		{
			auto& invuln = registry.get<GAME::Invulnerability>(ent);
			invuln.invulnPeriod -= deltaTime;
			if (invuln.invulnPeriod <= 0.0f)
			{
				registry.remove<GAME::Invulnerability>(ent);
				std::cout << "Enemy Boss is no longer invulnerable." << std::endl;
			}
		}
	}

	void UpdateEnemies(entt::registry& registry, entt::entity& entity)
	{
		double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;
		entt::basic_view activeGame = registry.view<GAME::Gaming>();
		entt::basic_view bossEnemies = registry.view<GAME::Enemy_Boss>();
		entt::basic_view lesserEnemies = registry.view<GAME::Enemy>();
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		entt::entity bossEntity{};

		for (auto ent : bossEnemies)
		{
			if (registry.all_of<GAME::BossTitle>(ent))
			{
				auto& title = registry.get<GAME::BossTitle>(ent);
				if (title.name == "EnemyBoss_Station") {

					UpdateBossOneBehavior(registry, ent);
				}
				else if (title.name == "EnemyBoss_UFO") {

					UpdateBossTwoBehavior(registry, ent);
				}
				else if (title.name == "EnemyBoss_RedRocket") {

					UpdateBossThreeBehavior(registry, ent);
				}

			}
			bossEntity = ent;
		}

		for (auto ent : activeGame)
		{

			// Enemy fly off screen
			const float flyOffDelay = (*config).at("Enemy1").at("flyOffTimer").as<float>();

			auto view = registry.view<GAME::Enemy, GAME::Transform, MoveTarget, AI::TimeAtPosition>();
			for (auto e : view)
			{
				auto& pos = registry.get<GAME::Transform>(e).matrix.row4;
				const auto& target = registry.get<MoveTarget>(e).pos;
				auto& timer = registry.get<AI::TimeAtPosition>(e);

				if (UTIL::Distance(target, pos) < 0.1f)
				{
					timer.timeAtPosition += deltaTime;
					if (timer.timeAtPosition >= flyOffDelay && !registry.any_of<AI::FlyOffScreen>(e))
					{
						EnemyFlyOffScreen(registry);
						timer.timeAtPosition = 0.0f;
					}
				}
				else
				{
					timer.timeAtPosition = 0.0f;
				}
			}

			for (auto ent : lesserEnemies)
			{
				GAME::Health hp = registry.get<GAME::Health>(ent);

				if (hp.health <= 0)
				{
					registry.emplace<GAME::Destroy>(ent);
					registry.remove<GAME::Enemy>(ent);
				}
			}

			if (registry.all_of<GAME::Health>(bossEntity))
			{
				auto& hp = registry.get<GAME::Health>(bossEntity);
				if (hp.health <= 0)
				{
					registry.emplace<GAME::Destroy>(bossEntity);
					registry.remove<GAME::SpawnEnemies>(bossEntity);
					registry.remove<GAME::Enemy_Boss>(bossEntity);
					std::cout << "Enemy Boss defeated!" << std::endl;
				}
			}
		}
	}

	void UpdateStandardProjectile(entt::registry& R) {
		const double dt = R.ctx().get<UTIL::DeltaTime>().dtSec;
		const auto& cfg = *R.ctx().get<UTIL::Config>().gameConfig;
		const float rate = cfg.at("Enemy1").at("firerate").as<float>();
		const float speed = cfg.at("EnemyBullet").at("speed").as<float>();
		const std::string model = cfg.at("EnemyBullet").at("model").as<std::string>();

		auto v = R.view<GAME::Enemy, GAME::Velocity, GAME::Transform, FormationMember>();

		for (auto e : v)
		{
			const GW::MATH::GVECTORF zero = GW::MATH::GIdentityVectorF;
			if (UTIL::Distance(zero, v.get<GAME::Velocity>(e).vec) > 0.01f) {
				if (auto* f = R.try_get<GAME::Firing>(e)) f->cooldown = rate;
				continue;
			}

			GAME::Firing* fire = R.try_get<GAME::Firing>(e);
			if (!fire) fire = &R.emplace<GAME::Firing>(e, rate);

			fire->cooldown -= dt;
			if (fire->cooldown > 0) continue;


			entt::entity b = R.create();
			R.emplace<GAME::Bullet>(b);
			R.emplace<GAME::BulletOwner>(b, e);

			const GW::MATH::GVECTORF dir = v.get<GAME::Transform>(e).matrix.row3;
			GW::MATH::GVECTORF velVec = { -dir.x * speed, -dir.y * speed, -dir.z * speed, 0 };

			UTIL::CreateVelocity(R, b, velVec, speed);
			UTIL::CreateTransform(R, b, v.get<GAME::Transform>(e).matrix);
			UTIL::CreateDynamicObjects(R, b, model);

			fire->cooldown = rate;
		}

	}

	void UpdateExplosions(entt::registry& registry)
	{
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		auto explosions = registry.view<AI::Explosion, GAME::Transform, AI::ExplosionGrowth>();

		for (auto ex : explosions)
		{
			auto& transform = explosions.get<GAME::Transform>(ex);
			auto& growth = explosions.get<AI::ExplosionGrowth>(ex);
			const float explosionDamage = (*config).at("Explosion").at("damage").as<float>();
			bool reached = UTIL::ScaleTowards(transform, growth.targetScale, growth.growthRate);

			if (reached)
			{
				registry.emplace_or_replace<GAME::Destroy>(ex);
			}
		}
	}

	void UpdateOrbAttack(entt::registry& registry)
	{
		auto orbView = registry.view<AI::OrbAttack, GAME::Transform, AI::OrbGrowth>();
		if (orbView.begin() == orbView.end()) return;

		auto players = registry.view<GAME::Player, GAME::Transform>();
		if (players.begin() == players.end()) return;

		const auto& playerTransform = registry.get<GAME::Transform>(*players.begin());
		const GW::MATH::GVECTORF playerPos = playerTransform.matrix.row4;

		const float dt = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
		const auto& cfg = *registry.ctx().get<UTIL::Config>().gameConfig;
		const float speed = cfg.at("Orb").at("speed").as<float>();

		for (auto e : orbView)
		{
			auto& T = orbView.get<GAME::Transform>(e);
			auto& growth = orbView.get<AI::OrbGrowth>(e);

			bool finishedGrowing = UTIL::ScaleTowards(T, growth.targetScale, growth.growthRate * dt);

			if (!finishedGrowing)
				continue;

			if (registry.any_of<GAME::Velocity>(e))
				continue;

			GW::MATH::GVECTORF dir;
			GW::MATH::GVector::SubtractVectorF(playerPos, T.matrix.row4, dir);
			GW::MATH::GVector::NormalizeF(dir, dir);

			GW::MATH::GVECTORF velScaled;
			GW::MATH::GVector::ScaleF(dir, speed, velScaled);

			registry.emplace<GAME::Velocity>(e, GAME::Velocity{ velScaled });
			registry.emplace<AI::OrbLifetime>(e, AI::OrbLifetime{ 6.f });
		}
	}
	void UpdateOrbLifetime(entt::registry& registry)
	{
		const double dt = registry.ctx().get<UTIL::DeltaTime>().dtSec;
		auto view = registry.view<AI::OrbAttack, AI::OrbLifetime>();
		if (view.begin() == view.end()) return;
		for (auto e : view)
		{
			auto& life = view.get<AI::OrbLifetime>(e);
			life.timeRemaining -= static_cast<float>(dt);
			if (life.timeRemaining <= 0.f)
				registry.emplace_or_replace<GAME::Destroy>(e);
		}
	}
	void UpdateKamikazeEnemy(entt::registry& registry)
	{

		entt::basic_view players = registry.view<GAME::Player, GAME::Transform>();
		entt::basic_view kamikazeEnemies = registry.view<AI::Kamikaze>();

		if (players.begin() == players.end() || kamikazeEnemies.empty()) return;

		auto& playerTransform = registry.get<GAME::Transform>(*players.begin());
		GW::MATH::GVECTORF playerPos = playerTransform.matrix.row4;

		// For each kamikaze enemy, move towards player
		for (auto ent : kamikazeEnemies)
		{
			if (!registry.all_of<GAME::Transform, GAME::Velocity>(ent)) continue;
			auto& enemyTransform = registry.get<GAME::Transform>(ent);
			auto& enemyVelocity = registry.get<GAME::Velocity>(ent);
			GW::MATH::GVECTORF enemyPos = enemyTransform.matrix.row4;
			GW::MATH::GVECTORF direction;
			GW::MATH::GVector::SubtractVectorF(playerPos, enemyPos, direction);
			GW::MATH::GVector::NormalizeF(direction, direction);
			float speed = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Kamikaze").at("speed").as<float>();
			GW::MATH::GVector::ScaleF(direction, speed, enemyVelocity.vec);

			// Rotate to face player
			UTIL::RotateTowards(enemyTransform, playerPos, G_DEGREE_TO_RADIAN_F(180.0f) * registry.ctx().get<UTIL::DeltaTime>().dtSec);

			// Get health of kamikaze enemy
			if (registry.any_of<GAME::Health>(ent))
			{
				auto& hp = registry.get<GAME::Health>(ent);
				if (hp.health <= 0)
				{
					Damage::Explosion(registry, ent);
					registry.emplace_or_replace<GAME::Destroy>(ent);
					registry.remove<GAME::Enemy>(ent);
				}
			}

			// If close enough to player, explode and destroy self
			if (UTIL::Distance(enemyPos, playerPos) < 1.0f)
			{
				Damage::Explosion(registry, ent);
				registry.emplace_or_replace<GAME::Destroy>(ent);
			}
		}
	}

	void UpdateLazerAttack(entt::registry& registry)
	{

		entt::basic_view lazerView = registry.view<AI::LazerSweep, GAME::Transform>();

		if (lazerView.begin() == lazerView.end()) return;
		float dt = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
		for (auto entity : lazerView)
		{
			auto& lazerTransform = lazerView.get<GAME::Transform>(entity);
			auto& lazerSweep = lazerView.get<AI::LazerSweep>(entity);
			float LENGTH_SCALE = 250.0f;
			float WIDTH_SCALE = 40.0f;
			UTIL::RotateTowards(lazerTransform, lazerSweep.pointB, G_DEGREE_TO_RADIAN_F(90) * dt);
			GW::MATH::GVector::ScaleF(lazerTransform.matrix.row3, LENGTH_SCALE, lazerTransform.matrix.row3);
			GW::MATH::GVector::ScaleF(lazerTransform.matrix.row1, WIDTH_SCALE, lazerTransform.matrix.row1);

			lazerSweep.duration -= dt;
			if (lazerSweep.duration <= 0.0f)
				registry.emplace_or_replace<GAME::Destroy>(entity);
		}
	}

	void UpdateBossSpawn(entt::registry& registry, entt::entity entity, unsigned int& bossWaveCount)
	{
		// Check if boss and enemy views are empty
		if (registry.view<GAME::Enemy>().empty() && registry.view<GAME::Enemy_Boss>().empty())
		{
			// access boss count from defaults.ini file and decrement
			std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
			auto bossCount = (*config).at("Player").at("bossCount").as<unsigned int>();

			if (bossWaveCount < bossCount)
			{
				if (bossWaveCount == 0)
					SpawnBoss(registry, "EnemyBoss_UFO");
				++bossWaveCount;
			}
			else if (bossWaveCount == bossCount)
			{
				SpawnFinalBoss(registry);
				bossWaveCount++;
			}
			else
			{
				// All bosses defeated, trigger game over
				registry.emplace_or_replace<GAME::GameOver>(registry.view<GAME::GameManager>().front(), GAME::GameOver{});
				std::cout << "You win, good job!" << std::endl;
			}
		}
	}

	void Update(entt::registry& registry, entt::entity entity)
	{
		static unsigned int bossWaveCount = 0;

		if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front()))
		{
			UpdateEnemies(registry, entity);
			UpdateKamikazeEnemy(registry);
			UpdateExplosions(registry);
			UpdateOrbAttack(registry);
			UpdateOrbLifetime(registry);
			UpdateLazerAttack(registry);
			UpdateFlockGoal(registry);
			UpdateFlock(registry);
			UpdateBossSpawn(registry, entity, bossWaveCount);
			UpdateMineDrones(registry);
			UpdateSpinningDrones(registry);
			UpdateFormation(registry);
			UpdateLocomotion(registry);
			UpdateStandardProjectile(registry);
			EnemyInvulnerability(registry, registry.ctx().get<UTIL::DeltaTime>().dtSec);
		}
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<AIDirector>().connect<Initialize>();
		registry.on_update<AIDirector>().connect<Update>();
	};
}
