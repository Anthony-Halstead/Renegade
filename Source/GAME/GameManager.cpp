#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../CCL.h"

namespace GAME
{
	// Helper functions

	// This takes in a name and location to update the player score that exists in the game manager.
	// It also checks whether this score is a new high score and replaces it if so.
	void UpdateScoreManager(entt::registry& registry, const std::string name, const std::string loc)
	{
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		entt::basic_view scoreManager = registry.view<Score>();
		for (auto scoreEnt : scoreManager)
		{
			auto& score = registry.get<Score>(scoreEnt);
			score.score += (*config).at(name).at(loc).as<unsigned>();
			if (score.score > score.highScore) score.highScore = score.score;
		}
	}

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
							// Prevent self-hit
							if (auto* owner = registry.try_get<BulletOwner>(ent)) {
								if (owner->owner == otherEnt || registry.any_of<Enemy>(otherEnt)) {
									// This bullet belongs to this enemy, skip
									continue;
								}
							}
							// Show current health of enemy
							std::cout << "Enemy current health: " << registry.get<Health>(otherEnt).health << std::endl;
							--registry.get<Health>(otherEnt).health;
							registry.emplace<Hit>(otherEnt);
							registry.emplace_or_replace<Destroy>(ent);
							std::cout << "Enemy hit by bullet. Current health: " << registry.get<Health>(otherEnt).health << std::endl;
						}
					}

					// Bullets hit enemy boss
					{
						if (registry.any_of<Bullet>(ent) && registry.any_of<Enemy_Boss>(otherEnt) 
							&& !registry.any_of<Hit>(otherEnt) && !registry.any_of<Invulnerability>(otherEnt))
						{
							// Prevent self-hit
							if (auto* owner = registry.try_get<BulletOwner>(ent)) {
								if (owner->owner == otherEnt || registry.any_of<Enemy_Boss>(otherEnt)) {
									// This bullet belongs to this enemy, skip
									continue;
								}
							}

							// Show current health of enemy boss
							// std::cout << "Enemy Boss current health: " << registry.get<Health>(otherEnt).health << std::endl;

							--registry.get<Health>(otherEnt).health;
							registry.emplace<Invulnerability>(otherEnt, (*registry.ctx().get<UTIL::Config>().gameConfig).at("EnemyBoss_Station").at("invulnPeriod").as<float>());
							registry.emplace<Hit>(otherEnt);
							registry.emplace_or_replace<Destroy>(ent);

							std::cout << "Enemy Boss hit by bullet. Current health: " << registry.get<Health>(otherEnt).health << std::endl;
							/*auto scoreView = registry.view<Score>();
							if (!scoreView.empty()) {
								auto scoreEnt = scoreView.front();
								std::cout << "Current Score: " << registry.get<Score>(scoreEnt).score << std::endl;
							} */
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
					if (registry.any_of<Bullet>(ent) && !registry.any_of<Destroy>(ent))
					{
						auto& bullet = registry.get<Bullet>(ent);
						bullet.lifetime -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
						if (bullet.lifetime <= 0.0f)
						{
							registry.emplace<Destroy>(ent);
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
			auto scoreView = registry.view<Score>();
			if (!scoreView.empty()) {
				auto scoreEnt = scoreView.front();
				std::cout << "Final score: " << registry.get<Score>(scoreEnt).score << std::endl;
				std::cout << "High score: " << registry.get<Score>(scoreEnt).highScore << std::endl;
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
			UpdatePosition(registry);
			UpdateCollide(registry);
			UpdateHit(registry);
			UpdatePlayers(registry, entity);
			UpdateDestroy(registry);
		}
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_update<GameManager>().connect<Update>();
	};
}