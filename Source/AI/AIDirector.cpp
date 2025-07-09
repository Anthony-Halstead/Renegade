#include "AIComponents.h"
#include "SpawnHelpers.h"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"
#include <random>
#include <algorithm>

namespace AI
{
	float LengthXZ(const GW::MATH::GVECTORF& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	void NormalizeXZ(const GW::MATH::GVECTORF& v, GW::MATH::GVECTORF& out)
	{
		float len = LengthXZ(v);
		out = (len > 1e-4f) ? GW::MATH::GVECTORF{ v.x / len, 0, v.z / len, 0 } : GW::MATH::GVECTORF{ 0,0,0,0 };
	}

	constexpr float DegToRad(float d) { return d * 0.0174532925f; }

	void YawToward(const GW::MATH::GVECTORF& desired, GW::MATH::GVECTORF& forward, float maxStepRad)
	{
		GW::MATH::GVECTORF d = { desired.x,0,desired.z,0 };
		GW::MATH::GVECTORF f = { forward.x,0,forward.z,0 };
		NormalizeXZ(d, d); NormalizeXZ(f, f);

		float dot; GW::MATH::GVector::DotF(d, f, dot);
		dot = std::clamp(dot, -1.f, 1.f);
		float angle = std::acos(dot);
		if (angle < 1e-4f) { forward = desired; return; }

		float crossY = d.x * f.z - d.z * f.x;
		float sign = (std::fabs(crossY) < 1e-4f) ? 1.f : (crossY < 0 ? -1.f : 1.f);

		float step = min(angle, maxStepRad) * sign;

		float c = std::cos(step), s = std::sin(step);
		forward = { f.x * c - f.z * s, 0, f.x * s + f.z * c, 0 };
	}

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

	void Initialize(entt::registry& registry)
	{
		SpawnBoss(registry, "EnemyBoss_Station");
	}

	void UpdateFormation(entt::registry& r)
	{
		auto view = r.view<FormationMember, MoveTarget, GAME::Transform, GAME::Velocity>();

		const float rotSpeedDeg = 90.f;
		const float rotStepRad = DegToRad(rotSpeedDeg) * r.ctx().get<UTIL::DeltaTime>().dtSec;

		for (auto e : view)
		{
			const auto& fm = view.get<FormationMember>(e);
			if (!r.valid(fm.anchor) || !r.all_of<FormationAnchor>(fm.anchor)) continue;
			const auto& anchor = r.get<FormationAnchor>(fm.anchor);
			if (fm.index >= anchor.slots.size()) continue;

			const GW::MATH::GVECTORF slotPos = anchor.slots[fm.index];
			const GW::MATH::GVECTORF anchorPos = anchor.origin;
			view.get<MoveTarget>(e).pos = slotPos;

			const GW::MATH::GMATRIXF& m = view.get<GAME::Transform>(e).matrix;
			GW::MATH::GVECTORF delta;
			GW::MATH::GVector::SubtractVectorF(slotPos, m.row4, delta);
			if (LengthXZ(delta) > 0.1f) continue;

			if (!r.any_of<CircleTag>(fm.anchor)) continue;

			GW::MATH::GVECTORF fwd = m.row3;

			GW::MATH::GVECTORF desired;
			GW::MATH::GVector::SubtractVectorF(anchorPos, slotPos, desired);
			NormalizeXZ(desired, desired);

			YawToward(desired, fwd, rotStepRad);

			const GW::MATH::GVECTORF up = { 0,1,0,0 };
			GW::MATH::GVECTORF right;  GW::MATH::GVector::CrossVector3F(up, fwd, right);
			GW::MATH::GVector::NormalizeF(right, right);
			GW::MATH::GVECTORF trueUp; GW::MATH::GVector::CrossVector3F(fwd, right, trueUp);

			GW::MATH::GMATRIXF& mw = view.get<GAME::Transform>(e).matrix;
			mw.row1 = { right.x,  right.y,  right.z, 0 };
			mw.row2 = { trueUp.x, trueUp.y, trueUp.z,0 };
			mw.row3 = fwd;
		}
	}
	void UpdateLocomotion(entt::registry& r)
	{
		auto view = r.view<MoveTarget, GAME::Velocity, GAME::Transform>();
		double dt = r.ctx().get<UTIL::DeltaTime>().dtSec;
		float speed = (*r.ctx().get<UTIL::Config>().gameConfig).at("Enemy1").at("speed").as<float>();

		for (auto e : view)
		{
			const auto& trg = view.get<MoveTarget>(e).pos;
			auto& vel = view.get<GAME::Velocity>(e).vec;
			auto& pos = view.get<GAME::Transform>(e).matrix.row4;

			GW::MATH::GVECTORF delta;
			GW::MATH::GVector::SubtractVectorF(trg, pos, delta);

			if (LengthXZ(delta) > 0.1f)
			{
				GW::MATH::GVECTORF dir; NormalizeXZ(delta, dir);
				GW::MATH::GVector::ScaleF(dir, speed, vel);
			}
			else
				vel = { 0,0,0,0 };
		}
	}
	void EnemyInvulnerability(entt::registry& registry, const float& deltaTime)
	{
		auto view = registry.view<GAME::Invulnerability>();
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
		entt::basic_view enemies = registry.view<GAME::Enemy_Boss, GAME::Health, GAME::SpawnEnemies>();
		entt::basic_view lesserEnemies = registry.view<GAME::Enemy, GAME::Health>();

		for (auto ent : enemies)
		{
			GAME::Health hp = registry.get<GAME::Health>(ent);

			auto& spawn = registry.get<GAME::SpawnEnemies>(ent);
			spawn.spawnTimer -= (float)deltaTime;

			entt::entity enemySpawn{};

			if (spawn.spawnTimer <= 0.0f)
			{
				spawn.spawnTimer = 10.0f;
				std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
				unsigned int bossHealth = (*config).at("EnemyBoss_Station").at("hitpoints").as<unsigned int>();

				entt::basic_view currentEnemies = registry.view<GAME::Enemy, GAME::Health>();

				if (bossHealth && currentEnemies.begin() == currentEnemies.end())
				{
					unsigned int maxEnemies = (*config).at("Enemy1").at("maxEnemies").as<unsigned int>();

					auto& bossTransform = registry.get<GAME::Transform>(ent);
					GW::MATH::GVECTORF bossPos = bossTransform.matrix.row4;

					SpawnWave(registry, RandomFormationType(), maxEnemies, bossPos, GW::MATH::GVECTORF{ 0,-2,0,1 }, 10);
				}
			}

			for (auto ent : lesserEnemies)
			{
				GAME::Health hp = registry.get<GAME::Health>(ent);

				if (hp.health <= 0) registry.emplace<GAME::Destroy>(ent);
			}

			if (hp.health <= 0)
			{
				registry.emplace<GAME::Destroy>(ent);
				registry.remove<GAME::SpawnEnemies>(ent);
				registry.remove<GAME::Enemy_Boss>(ent);
				std::cout << "Enemy Boss defeated!" << std::endl;
			}
			else
			{
				registry.patch<GAME::Enemy_Boss>(ent);
			}
		}

		entt::basic_view enemiesLeft = registry.view<GAME::Enemy_Boss>();
		if (enemiesLeft.empty())
		{
			registry.emplace_or_replace<GAME::GameOver>(entity);
			std::cout << "You win, good job!" << std::endl;
			auto scoreView = registry.view<GAME::Score>();
			if (!scoreView.empty()) {
				auto scoreEnt = scoreView.front();
				std::cout << "Final Score: " << registry.get<GAME::Score>(scoreEnt).score << std::endl;
				std::cout << "High Score: " << registry.get<GAME::Score>(scoreEnt).highScore << std::endl;
			}
		}
	}
	void UpdateEnemyAttack(entt::registry& R)
	{
		const double dt = R.ctx().get<UTIL::DeltaTime>().dtSec;
		const auto& cfg = *R.ctx().get<UTIL::Config>().gameConfig;
		const float rate = cfg.at("Enemy1").at("firerate").as<float>();
		const float speed = cfg.at("Bullet").at("speed").as<float>();
		const std::string model = cfg.at("Bullet").at("model").as<std::string>();

		auto v = R.view<GAME::Enemy, GAME::Velocity, GAME::Transform>();

		for (auto e : v)
		{
			if (LengthXZ(v.get<GAME::Velocity>(e).vec) > 0.01f) {
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
	void Update(entt::registry& registry, entt::entity entity)
	{
		if (!registry.any_of<GAME::GameOver>(entity))
		{
			UpdateEnemies(registry, entity);
			UpdateFormation(registry);
			UpdateLocomotion(registry);
			UpdateEnemyAttack(registry);
			EnemyInvulnerability(registry, registry.ctx().get<UTIL::DeltaTime>().dtSec);
		}		
	}


	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<AIDirector>().connect<Initialize>();
		registry.on_update<AIDirector>().connect<Update>();
	};
}
