#include "AIComponents.h"
#include "SpawnHelpers.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"
#include "../APP/Window.hpp"
#include <random>
#include <algorithm>

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
			auto& T = registry.get<GAME::Transform>(e).matrix;
			auto& V = registry.get<GAME::Velocity>(e);
			auto& stats = registry.get<BoidStats>(e);

			auto align = CalculateAlignment(avgFwd, stats, stats.alignmentStrength);
			auto coh = CalculateCohesion(avgPos, T.row4, stats.flockRadius, stats.cohesionStrength);
			auto sep = CalculateSeparation(registry, e, stats.safeRadius, stats.separationStrength);
			auto seek = CalculateSeek(T.row4, goal.pos, stats.maxSpeed, stats.seekStrength);
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

	void UpdateBossOneBehavior(entt::registry& registry, entt::entity& entity)
	{
		double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;
		entt::basic_view enemies = registry.view<GAME::Enemy_Boss, GAME::Health, GAME::SpawnEnemies>();

		for (auto ent : enemies)
		{
			GAME::Health hp = registry.get<GAME::Health>(ent);

			auto& spawn = registry.get<GAME::SpawnEnemies>(ent);
			spawn.spawnTimer -= (float)deltaTime;

			entt::entity enemySpawn{};

			if (spawn.spawnTimer <= 0.0f)
			{
				spawn.spawnTimer = 20.0f;
				std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
				unsigned int bossHealth = (*config).at("EnemyBoss").at("hitpoints").as<unsigned int>();

				if (bossHealth)
				{
					unsigned int maxEnemies = (*config).at("Enemy1").at("maxEnemies").as<unsigned int>();

					auto& bossTransform = registry.get<GAME::Transform>(ent);
					GW::MATH::GVECTORF bossPos = bossTransform.matrix.row4;
					//	SpawnFlock(registry, 20, bossPos);
					SpawnWave(registry, RandomFormationType(), 8, bossPos, GW::MATH::GVECTORF{ 0,-2,0,1 }, 10);
				}

				if (hp.health <= 0)
				{
					registry.emplace<GAME::Destroy>(ent);
					registry.remove<GAME::SpawnEnemies>(ent);
					registry.remove<GAME::Enemy_Boss>(ent);
					std::cout << "Enemy Boss defeated!" << std::endl;
				}

				entt::basic_view lesserEnemies = registry.view<GAME::Enemy, GAME::Health>();

				for (auto ent : lesserEnemies)
				{
					GAME::Health hp = registry.get<GAME::Health>(ent);

					if (hp.health <= 0) registry.emplace<GAME::Destroy>(ent);
				}
			}
		}

		entt::basic_view enemiesLeft = registry.view<GAME::Enemy_Boss>();

		if (enemiesLeft.empty())
		{
			registry.emplace<GAME::GameOver>(entity);
			std::cout << "You win, good job!" << std::endl;
		}
	}
	void UpdateBossTwoBehavior(entt::registry& registry, entt::entity& entity)
	{

	}
	void UpdateBossThreeBehavior(entt::registry& registry, entt::entity& entity)
	{

	}

	void Initialize(entt::registry& registry)
	{
		SpawnBoss(registry, "EnemyBoss_Station");
	}

	void UpdateFormation(entt::registry& r)
	{
		auto view = r.view<FormationMember, MoveTarget, GAME::Transform, GAME::Velocity>();

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
		auto view = r.view<MoveTarget, GAME::Velocity, GAME::Transform>();
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
		unsigned int bossHealth = (*config).at("EnemyBoss_Station").at("hitpoints").as<unsigned int>();
		unsigned int bossCount = (*config).at("Player").at("bossCount").as<unsigned int>();

		entt::entity bossEntity{};

		for (auto ent : bossEnemies)
		{
			bossEntity = ent;
		}

		for (auto ent : activeGame)
		{
			if (registry.all_of<GAME::SpawnEnemies>(bossEntity))
			{
				auto& spawn = registry.get<GAME::SpawnEnemies>(bossEntity);
				spawn.spawnTimer -= (float)deltaTime;

				if (spawn.spawnTimer <= 0.0f)
				{
					spawn.spawnTimer = 10.0f;

					entt::basic_view currentEnemies = registry.view<GAME::Enemy>();

					if (bossHealth && currentEnemies.empty())
					{
						unsigned int maxEnemies = (*config).at("Enemy1").at("maxEnemies").as<unsigned int>();

						auto& bossTransform = registry.get<GAME::Transform>(bossEntity);
						GW::MATH::GVECTORF bossPos = bossTransform.matrix.row4;
						//SpawnFlock(registry, 20, bossPos);
						SpawnWave(registry, RandomFormationType(), maxEnemies, bossPos, GW::MATH::GVECTORF{ 0,-2,0,1 }, 10);


					}
				}
			}

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
						//registry.emplace_or_replace<GAME::FlyOffScreen>(e);
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
				else
				{
					registry.patch<GAME::Enemy_Boss>(bossEntity);
				}
			}
		}
	}

	void UpdateStandardProjectile(entt::registry& R) {
		const double dt = R.ctx().get<UTIL::DeltaTime>().dtSec;
		const auto& cfg = *R.ctx().get<UTIL::Config>().gameConfig;
		const float rate = cfg.at("Enemy1").at("firerate").as<float>();
		const float speed = cfg.at("Bullet").at("speed").as<float>();
		const std::string model = cfg.at("Bullet").at("model").as<std::string>();

		auto v = R.view<GAME::Enemy, GAME::Velocity, GAME::Transform>();

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

	void UpdateAIDestroy(entt::registry& registry)
	{
		entt::basic_view destroy = registry.view<GAME::Destroy>();
		for (auto ent : destroy) registry.destroy(ent);
	}

	void Update(entt::registry& registry, entt::entity entity)
	{
		static unsigned int bossWaveCount = 0;

		if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front()))
		{
			std::size_t active = registry.view<DRAW::OBB, GAME::Collidable>().size_hint();
			std::cout << "Active collidables: " << active << '\n';
			UpdateEnemies(registry, entity);

			UpdateFlockGoal(registry);
			UpdateFlock(registry);



			// Check if boss and enemy views are empty
			if (registry.view<GAME::Enemy>().empty() && registry.view<GAME::Enemy_Boss>().empty())
			{
				// access boss count from defaults.ini file and decrement
				std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
				auto bossCount = (*config).at("Player").at("bossCount").as<unsigned int>();
				//SpawnBoss(registry, "EnemyBoss_UFO");

				if (bossWaveCount < bossCount)
				{
					if (bossWaveCount == 0)
						SpawnBoss(registry, "EnemyBoss_UFO");
					++bossWaveCount;
				}
				else
				{
					// All bosses defeated, trigger game over
					registry.emplace_or_replace<GAME::GameOver>(registry.view<GAME::GameManager>().front(), GAME::GameOver{});
					std::cout << "You win, good job!" << std::endl;
				}
			}

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
