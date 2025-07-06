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
						if (registry.any_of<Bullet>(ent) && registry.any_of<Enemy_Boss>(otherEnt) && !registry.any_of<Hit>(otherEnt))
						{
							//// Prevent self-hit
							//if (auto* owner = registry.try_get<BulletOwner>(ent)) {
							//	if (owner->owner == otherEnt) {
							//		// This bullet belongs to this enemy, skip
							//		continue;
							//	}
							//}

							// Show current health of enemy boss
							std::cout << "Enemy Boss current health: " << registry.get<Health>(otherEnt).health << std::endl;

							--registry.get<Health>(otherEnt).health;
							registry.emplace<Hit>(otherEnt);
							registry.emplace_or_replace<Destroy>(ent);

							std::cout << "Enemy Boss hit by bullet. Current health: " << registry.get<Health>(otherEnt).health << std::endl;
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
				else
				{
					// Destroy bullet after 5 seconds
					if (registry.any_of<Bullet>(ent) && !registry.any_of<Destroy>(ent))
					{
						auto& bullet = registry.get<Bullet>(ent);
						bullet.lifetime -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
						if (bullet.lifetime <= 0.0f)
						{
							registry.emplace<Destroy>(ent);
							std::cout << "Bullet destroyed after lifetime expired." << std::endl;
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
		entt::basic_view enemies = registry.view<Enemy_Boss, Health, SpawnEnemies>();		

		for (auto ent : enemies)
		{
			Health hp = registry.get<Health>(ent);

			// timer currently used to test spawning new enemies
			auto& spawn = registry.get<SpawnEnemies>(ent);
			spawn.spawnTimer -= (float)deltaTime;

			entt::entity enemySpawn{};

			if (spawn.spawnTimer <= 0.0f)
			{
				std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
				unsigned bossHealth = (*config).at("EnemyBoss").at("hitpoints").as<unsigned>();

				if (bossHealth)
				{
					std::string enemyModel = (*config).at("Enemy1").at("model").as<std::string>();
					unsigned enemyHealth = (*config).at("Enemy1").at("hitpoints").as<unsigned>();
					unsigned maxEnemies = (*config).at("Enemy1").at("maxEnemies").as<unsigned>();
					unsigned currentEnemyCount = static_cast<unsigned>(registry.view<Enemy>().size());

					// Get the boss's position
					auto& bossTransform = registry.get<Transform>(ent);
					GW::MATH::GVECTORF bossPos = bossTransform.matrix.row4;

					// Define the relative positions for each small enemy
					std::vector<GW::MATH::GVECTORF> enemyOffsets = {
						{ -15.0f, 0.0f, -26.0f, 1.0f },
						{ -8.0f, 0.0f, -28.0f, 1.0f },
						{ 0.0f, 0.0f, -30.0f, 1.0f },
						{ 8.0f, 0.0f, -28.0f, 1.0f },
						{ 15.0f, 0.0f, -26.0f, 1.0f }
					};

					for (unsigned i = 0; i < maxEnemies && currentEnemyCount < maxEnemies; ++i, ++currentEnemyCount)
					{
						entt::entity enemySpawn = registry.create();

						registry.emplace<GAME::Enemy>(enemySpawn);
						registry.emplace<GAME::Health>(enemySpawn, enemyHealth);

						// Set enemy state to Moving
						registry.emplace<GAME::EnemyState>(enemySpawn, GAME::EnemyState{ GAME::EnemyState::State::Moving });
						// Debug log to show moving state
						std::cout << "Enemy " << currentEnemyCount << " spawned with state Moving." << std::endl;

						// Set initial position at the boss
						GW::MATH::GMATRIXF enemyMatrix = bossTransform.matrix;
						registry.emplace<GAME::Transform>(enemySpawn, enemyMatrix);

						// Calculate and set the target position
						GW::MATH::GVECTORF targetPos;
						GW::MATH::GVector::AddVectorF(bossPos, enemyOffsets[i], targetPos);
						registry.emplace<GAME::TargetPosition>(enemySpawn, targetPos);

						// Give a velocity component (start at zero)
						registry.emplace<GAME::Velocity>(enemySpawn, GW::MATH::GVECTORF{ 0,0,0,0 });

						UTIL::CreateDynamicObjects(registry, enemySpawn, enemyModel);
						std::cout << "Spawned enemy at: "
							<< enemyMatrix.row4.x << ", "
							<< enemyMatrix.row4.y << ", "
							<< enemyMatrix.row4.z << " | Health: "
							<< enemyHealth << std::endl;

						// This of a way to have a wave of enemies appear on the side of the boss and 
						// face towards the player before they begin attacking

						// Possibly also have those same enemies take and fly towards the player kamikaze style 
						// but have them only fly through and damage the player and not destroy themselves 
						// before flying off screen and flying back into their original positions
					}
				}

				// Check if the boss health is zero or less
				if (hp.health <= 0)
				{
					registry.emplace<Destroy>(ent);
					registry.remove<SpawnEnemies>(ent);
					registry.remove<Enemy_Boss>(ent);
					std::cout << "Enemy Boss defeated!" << std::endl;
				}

				// Tracks list of small enemies spawned from large boss
				entt::basic_view lesserEnemies = registry.view<Enemy, Health>();

				for (auto ent : lesserEnemies)
				{
					Health hp = registry.get<Health>(ent);

					if (hp.health <= 0) registry.emplace<Destroy>(ent);
				}
			}
		}

		entt::basic_view enemiesLeft = registry.view<Enemy_Boss>();

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

	void UpdateEnemyMovement(entt::registry& registry)
	{
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		auto view = registry.view<Enemy, Transform, TargetPosition, Velocity>();
		for (auto ent : view)
		{
			auto& transform = registry.get<Transform>(ent);
			auto& target = registry.get<TargetPosition>(ent);
			auto& velocity = registry.get<Velocity>(ent);

			GW::MATH::GVECTORF& pos = transform.matrix.row4;
			GW::MATH::GVECTORF dir;
			GW::MATH::GVector::SubtractVectorF(target.position, pos, dir);

			float distance;
			GW::MATH::GVector::MagnitudeF(dir, distance);

			if (distance <= 1.0f) {
				// Arrived: stop movement
				velocity.vec = { 0, 0, 0, 0 };
				registry.remove<TargetPosition>(ent); // Remove to stop further updates

				// Set enemy state to Ready
				if (auto* state = registry.try_get<EnemyState>(ent)) {
					state->state = EnemyState::State::Ready;
					// Debug log to show ready state
					std::cout << "Enemy " << static_cast<uint32_t>(ent) << " reached target and is now Ready." << std::endl;
				}
			}
			else {
				// Move toward target (normalize direction and set speed)
				GW::MATH::GVector::NormalizeF(dir, dir);
				float speed = (*config).at("Enemy1").at("speed").as<float>();
				velocity.vec = { dir.x * speed, dir.y * speed, dir.z * speed, 0 };
			}
		}
	}

	void UpdateEnemyAttack(entt::registry& registry)
	{
		auto view = registry.view<Enemy, EnemyState>();
		double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;

		for (auto ent : view)
		{
			auto& state = registry.get<EnemyState>(ent);
			if (state.state == EnemyState::State::Ready || state.state == EnemyState::State::Attacking)
			{
				auto& enemyStats = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Enemy1");
				float fireRate = enemyStats.at("firerate").as<float>();

				GAME::Firing* isFiring = registry.try_get<GAME::Firing>(ent);

				if (isFiring != nullptr)
				{
					isFiring->cooldown -= deltaTime;
					if (isFiring->cooldown <= 0.0f)
					{
						// Fire logic here, e.g., create a bullet entity
						entt::entity bullet = registry.create();
						registry.emplace<GAME::Bullet>(bullet);
						registry.emplace<GAME::BulletOwner>(bullet, ent); // Set owner of the bullet
						std::string bulletModel = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Bullet").at("model").as<std::string>();
						float bulletSpeed = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Bullet").at("speed").as<float>();
						GW::MATH::GVECTORF velocity = { 0, 0, -bulletSpeed, 0 }; // Example direction
						UTIL::CreateVelocity(registry, bullet, velocity, bulletSpeed);
						UTIL::CreateTransform(registry, bullet, registry.get<Transform>(ent).matrix);
						UTIL::CreateDynamicObjects(registry, bullet, bulletModel);
						isFiring->cooldown = fireRate; // Reset cooldown

						// Add logic here to have the enemies fire 5 bullets then wait a bit before firing again

						// Set state to Attacking after first bullet
						state.state = EnemyState::State::Attacking;
						std::cout << "Enemy " << static_cast<uint32_t>(ent) << " is now Attacking." << std::endl;
					}
				}
				else
				{
					// Start firing
					registry.emplace<GAME::Firing>(ent, fireRate);
				}
			}
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
			UpdateEnemyMovement(registry);
			UpdatePosition(registry);
			UpdateCollide(registry);
			UpdateHit(registry);
			UpdateEnemies(registry, entity);
			UpdateEnemyAttack(registry);
			UpdatePlayers(registry, entity);
			UpdateDestroy(registry);
		}
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_update<GameManager>().connect<Update>();
	};
}