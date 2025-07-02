#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../CCL.h"

namespace GAME
{
	// Helper functions

	void SetUpOBB(GW::MATH::GOBBF& obb, Transform& transform)
	{
		GW::MATH::GQUATERNIONF rotation;
		GW::MATH::GVECTORF scale;

		GW::MATH::GMatrix::GetScaleF(transform.matrix, scale);

		obb.extent.x *= scale.x;
		obb.extent.y *= scale.y;
		obb.extent.z *= scale.z;

		obb.center = transform.matrix.row4;

		GW::MATH::GQuaternion::SetByMatrixF(transform.matrix, rotation);
		GW::MATH::GQuaternion::MultiplyQuaternionF(obb.rotation, rotation, obb.rotation);

	}

	void BounceEnemy(const GW::MATH::GVECTORF& transform, GW::MATH::GVECTORF& velocity, const GW::MATH::GOBBF& obb)
	{
		float dot;

		GW::MATH::GVECTORF point;
		GW::MATH::GVECTORF normal;

		GW::MATH::GCollision::ClosestPointToOBBF(obb, transform, point);
		GW::MATH::GVector::SubtractVectorF(transform, point, normal);

		normal.y = normal.w = 0.f;

		GW::MATH::GVector::NormalizeF(normal, normal);
		GW::MATH::GVector::DotF(velocity, normal, dot);

		dot *= 2.f;

		GW::MATH::GVector::ScaleF(normal, dot, normal);
		GW::MATH::GVector::SubtractVectorF(velocity, normal, velocity);
	}

	void BlockPlayer(GW::MATH::GVECTORF& transform, const GW::MATH::GOBBF& obb, const float& speed, const float& deltaTime)
	{
		GW::MATH::GVECTORF point;
		GW::MATH::GVECTORF normal;

		GW::MATH::GCollision::ClosestPointToOBBF(obb, transform, point);
		GW::MATH::GVector::SubtractVectorF(transform, point, normal);

		normal.y = normal.w = 0.f;

		GW::MATH::GVector::NormalizeF(normal, normal);
		GW::MATH::GVector::ScaleF(normal, speed * deltaTime, normal);
		GW::MATH::GVector::AddVectorF(transform, normal, transform);
	}

	// Update functions

	void UpdatePosition(entt::registry& registry) {
		entt::basic_view ents = registry.view<Transform, DRAW::MeshCollection>();
		double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;

		for (auto ent : ents)
		{
			Transform& transform = registry.get<Transform>(ent);
			Velocity* velocity = registry.try_get<Velocity>(ent);

			if (velocity != nullptr)
			{
				GW::MATH::GVECTORF& translate = transform.matrix.row4;

				translate.x += velocity->vec.x * deltaTime;
				translate.y += velocity->vec.y * deltaTime;
				translate.z += velocity->vec.z * deltaTime;
			}

			DRAW::MeshCollection meshes = registry.get<DRAW::MeshCollection>(ent);

			for (auto mesh : meshes.entities)
			{
				DRAW::GPUInstance& gpuInstance = registry.get<DRAW::GPUInstance>(mesh);
				gpuInstance.transform = transform.matrix;
			}
		}
	}

	void UpdateCollide(entt::registry& registry)
	{
		entt::basic_view collidables = registry.view<Transform, DRAW::MeshCollection, Collidable>();

		for (auto ent : collidables)
		{
			for (auto otherEnt : collidables)
			{
				if (otherEnt == ent) continue;

				GW::MATH::GOBBF obb = registry.get<DRAW::MeshCollection>(ent).obb;
				Transform& transform = registry.get<Transform>(ent);

				SetUpOBB(obb, transform);

				GW::MATH::GOBBF otherObb = registry.get<DRAW::MeshCollection>(otherEnt).obb;
				Transform otherTransform = registry.get<Transform>(otherEnt);

				SetUpOBB(otherObb, otherTransform);

				GW::I::GCollisionInterface::GCollisionCheck hasCollided;
				GW::MATH::GCollision::TestOBBToOBBF(obb, otherObb, hasCollided);

				if (hasCollided == GW::I::GCollisionInterface::GCollisionCheck::COLLISION)
				{
					// Bullets hit wall
					{
						if (registry.any_of<Bullet>(ent) && registry.any_of<Obstacle>(otherEnt)) registry.emplace_or_replace<Destroy>(ent);
						if (registry.any_of<Bullet>(otherEnt) && registry.any_of<Obstacle>(ent)) registry.emplace_or_replace<Destroy>(otherEnt);
					}

					// Player hit wall
					{
						if (registry.any_of<Player>(ent) && registry.any_of<Obstacle>(otherEnt)) BlockPlayer(transform.matrix.row4, otherObb, (*registry.ctx().get<UTIL::Config>().gameConfig).at("Player").at("speed").as<float>(), registry.ctx().get<UTIL::DeltaTime>().dtSec);
					}

					// Enemy hit wall
					{
						if (registry.any_of<Enemy>(ent) && registry.any_of<Obstacle>(otherEnt))
						{
							GW::MATH::GVECTORF& velocity = registry.get<Velocity>(ent).vec;
							BounceEnemy(transform.matrix.row4, velocity, otherObb);
						}
					}

					// Bullets hit enemy
					{
						if (registry.any_of<Bullet>(ent) && registry.any_of<Enemy>(otherEnt) && !registry.any_of<Hit>(otherEnt))
						{
							--registry.get<Health>(otherEnt).health;
							registry.emplace<Hit>(otherEnt);
							registry.emplace_or_replace<Destroy>(ent);
						}
					}

					// Player hit enemy
					{
						if (registry.any_of<Player>(ent) && registry.any_of<Enemy>(otherEnt) && !registry.any_of<Invulnerability>(ent))
						{
							--registry.get<Health>(ent).health;
							registry.emplace<Invulnerability>(ent, (*registry.ctx().get<UTIL::Config>().gameConfig).at("Player").at("invulnPeriod").as<float>());
							std::cout << "Player's current health: " << registry.get<Health>(ent).health << std::endl;
						}
					}
				}
			}
		}
	}

	void UpdateHit(entt::registry& registry)
	{
		entt::basic_view hit = registry.view<Hit>();
		registry.remove<Hit>(hit.begin(), hit.end());
	}

	void UpdateEnemies(entt::registry& registry, entt::entity& entity)
	{
		double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;
		entt::basic_view enemies = registry.view<Enemy, Health, SpawnEnemies>();		

		for (auto ent : enemies)
		{
			Health hp = registry.get<Health>(ent);

			// timer currently used to test spawning new enemies
			auto& spawn = registry.get<SpawnEnemies>(ent);
			spawn.spawnTimer -= (float)deltaTime;

			if (spawn.spawnTimer <= 0.0f)
			{
				std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
				unsigned enemyShatter = registry.get<Shatters>(ent).shatterCount - 1;

				if (enemyShatter)
				{
					std::string enemyModel = (*config).at("Enemy1").at("model").as<std::string>();

					float enemySpeed = (*config).at("Enemy1").at("speed").as<float>();
					float enemyShatterScale = (*config).at("Enemy1").at("shatterScale").as<float>();
					unsigned enemyHealth = (*config).at("Enemy1").at("hitpoints").as<unsigned>();
					unsigned shatterAmount = (*config).at("Enemy1").at("shatterAmount").as<unsigned>();

					unsigned maxEnemies = (*config).at("Enemy1").at("maxEnemies").as<unsigned>();
					unsigned currentEnemyCount = static_cast<unsigned>(registry.view<Enemy>().size());

					GW::MATH::GMATRIXF transform = registry.get<Transform>(ent).matrix;
					GW::MATH::GVECTORF vec = { 1, 1, 1, 1 };

					GW::MATH::GVector::ScaleF(vec, enemyShatterScale, vec);
					GW::MATH::GMatrix::ScaleGlobalF(transform, vec, transform);

					for (unsigned i = 0; i < shatterAmount && currentEnemyCount < maxEnemies; ++i)
					{
						entt::entity enemy = registry.create();

						registry.emplace<GAME::Enemy>(enemy);
						registry.emplace<GAME::Health>(enemy, enemyHealth);
						registry.emplace<GAME::Shatters>(enemy, enemyShatter);
						registry.emplace<GAME::SpawnEnemies>(enemy, 2.0f);

						UTIL::CreateVelocity(registry, enemy, UTIL::GetRandomVelocityVector(), enemySpeed);
						UTIL::CreateTransform(registry, enemy, transform);
						UTIL::CreateDynamicObjects(registry, enemy, enemyModel);
					}
				}
				//registry.emplace<Destroy>(ent);
			}
		}

		entt::basic_view enemiesLeft = registry.view<Enemy>();

		if (enemiesLeft.empty())
		{
			registry.emplace<GameOver>(entity);
			std::cout << "You win, good job!" << std::endl;
		}
	}

	void UpdatePlayers(entt::registry& registry, const entt::entity& entity)
	{
		entt::basic_view players = registry.view<Player>();
		unsigned deathCount = 0;

		for (auto ent : players)
		{
			if (registry.get<Health>(ent).health <= 0) ++deathCount;
			else registry.patch<Player>(ent);
		}

		if (deathCount == players.size())
		{
			registry.emplace<GameOver>(entity);
			std::cout << "You lose, game over." << std::endl;
		}
	}

	void UpdateDestroy(entt::registry& registry)
	{
		entt::basic_view destroy = registry.view<Destroy>();
		for (auto ent : destroy) registry.destroy(ent);
	}

	void Update(entt::registry& registry, entt::entity entity) 
	{
		if (!registry.any_of<GameOver>(entity))
		{
			UpdatePosition(registry);
			UpdateCollide(registry);
			UpdateHit(registry);
			UpdateEnemies(registry, entity);
			UpdatePlayers(registry, entity);
			UpdateDestroy(registry);
		}
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_update<GameManager>().connect<Update>();
	};
}