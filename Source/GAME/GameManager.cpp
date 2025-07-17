#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../CCL.h"
#include "../APP/Window.hpp"
#include "../UI/UIComponents.h"
#include <iostream>
#include "ItemPickupComponents.h"
#include "../AI/AIComponents.h"

namespace GAME
{
	// Helper functions

	void UpdateClampToScreen(entt::registry& registry, float marginOffset = 23.f)
	{
		auto wView = registry.view<APP::Window>();
		if (wView.empty()) return;
		const auto& window = wView.get<APP::Window>(*wView.begin());

		float halfWidth = static_cast<float>(window.width) / marginOffset;
		float halfHeight = static_cast<float>(window.height) / marginOffset;

		float minX = -halfWidth;
		float maxX = halfWidth;
		float minZ = -halfHeight;
		float maxZ = halfHeight;

		auto v = registry.view<GAME::Transform, GAME::Bounded>();
		for (auto e : v)
		{
			auto& M = v.get<GAME::Transform>(e).matrix;
			if (M.row4.x < minX)   M.row4.x = minX;
			if (M.row4.x > maxX)   M.row4.x = maxX;
			if (M.row4.z < minZ)   M.row4.z = minZ;
			if (M.row4.z > maxZ)   M.row4.z = maxZ;
		}
	}

	/*void SetUpOBB(GW::MATH::GOBBF& obb, Transform& transform)
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

	}*/

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
	void UpdatePosition(entt::registry& reg)
	{
	 const double dt = reg.ctx().get<UTIL::DeltaTime>().dtSec;

		auto movers = reg.view<Transform, DRAW::MeshCollection>();
		for (auto e : movers)
		{
			auto& T = movers.get<Transform>(e);
			if (auto* V = reg.try_get<Velocity>(e))
			{
				auto& p = T.matrix.row4;
				p.x += V->vec.x * dt;
				p.y += V->vec.y * dt;
				p.z += V->vec.z * dt;
			}

			if (reg.all_of<DRAW::OBB>(e))
			{
				const auto& local = movers.get<DRAW::MeshCollection>(e).obb;
				reg.get<DRAW::OBB>(e).obb = UTIL::BuildOBB(local, T);
			}

			auto& collection = movers.get<DRAW::MeshCollection>(e);
			for (auto it = collection.entities.begin(); it != collection.entities.end(); /* manual ++ */)
			{
				if (!reg.valid(*it)) {
					it = collection.entities.erase(it);
					continue;
				}

				entt::entity mesh = *it; ++it;
				if (reg.all_of<DRAW::GPUInstance>(mesh))
					reg.get<DRAW::GPUInstance>(mesh).transform = T.matrix;
			}
		}
  }

	void UpdateCollide(entt::registry& reg)
	{
		const float dt = reg.ctx().get<UTIL::DeltaTime>().dtSec;

		auto colliders = reg.view<DRAW::OBB, Collidable>();

		auto HandlePair = [&](entt::entity a, entt::entity b)
			{

				if (reg.all_of<Bullet>(a) && reg.all_of<Enemy>(b) &&
					!reg.any_of<Hit>(b))
				{
					if (auto* o = reg.try_get<BulletOwner>(a))
						if (o->owner == b || !reg.any_of<Player>(o->owner)) return;

					--reg.get<Health>(b).health;
					reg.emplace<Hit>(b);
					GW::MATH::GVECTORF pos = reg.get<Transform>(b).matrix.row4;
					GAME::HandlePickup(reg, pos);
					reg.emplace_or_replace<Destroy>(a);
					return;
				}

				if (reg.all_of<Bullet>(a) && reg.all_of<Enemy_Boss>(b) &&
					!reg.any_of<Hit>(b) && !reg.any_of<Invulnerability>(b))
				{
					if (auto* o = reg.try_get<BulletOwner>(a))
						if (o->owner == b || !reg.any_of<Player>(o->owner)) return;

					--reg.get<Health>(b).health;
					reg.emplace<Invulnerability>(b,
						reg.ctx().get<UTIL::Config>().gameConfig->at("EnemyBoss_Station")
						.at("invulnPeriod").as<float>());
					reg.emplace<Hit>(b);
					reg.emplace_or_replace<Destroy>(a);
					return;
				}

				if (reg.all_of<Bullet>(a) && reg.all_of<Player>(b) &&
					!reg.any_of<Hit>(b) && !reg.any_of<Invulnerability>(b))
				{
					if (auto* o = reg.try_get<BulletOwner>(a))
						if (o->owner == b) return;

					--reg.get<Health>(b).health;
					reg.emplace<Invulnerability>(b,
						reg.ctx().get<UTIL::Config>().gameConfig->at("Player")
						.at("invulnPeriod").as<float>());
					reg.emplace<Hit>(b);
					reg.emplace_or_replace<Destroy>(a);
					return;
				}

				// Prevents damage from kamikaze enemies colliding with player, only explosion damages player
				if (reg.all_of<Player>(a) && reg.all_of<Enemy>(b) &&
					!reg.any_of<Invulnerability>(a) && !reg.any_of<AI::Kamikaze>(b))
				{
					--reg.get<Health>(a).health;
					reg.emplace<Invulnerability>(a,
						reg.ctx().get<UTIL::Config>().gameConfig->at("Player")
						.at("invulnPeriod").as<float>());
					std::cout << "Player's current health: "
						<< reg.get<Health>(a).health << '\n';
				}

				// Explosion damage
				if (reg.all_of<Player>(a) && reg.all_of<AI::Explosion>(b) &&
					!reg.any_of<Invulnerability>(a))
				{
					--reg.get<Health>(a).health;
					reg.emplace<Invulnerability>(a,
						reg.ctx().get<UTIL::Config>().gameConfig->at("Player")
						.at("invulnPeriod").as<float>());
					std::cout << "Player's current health: "
						<< reg.get<Health>(a).health << '\n';
				}

				// Orb attack
				if (reg.all_of<Player>(a) && reg.all_of<AI::OrbAttack>(b) &&
					!reg.any_of<Invulnerability>(a))
				{
					--reg.get<Health>(a).health;
					reg.emplace<Invulnerability>(a,
						reg.ctx().get<UTIL::Config>().gameConfig->at("Player")
						.at("invulnPeriod").as<float>());
					std::cout << "Player's current health: "
						<< reg.get<Health>(a).health << '\n';
				}

			};

		for (auto itA = colliders.begin(); itA != colliders.end(); ++itA)
		{
			const auto& obbA = colliders.get<DRAW::OBB>(*itA).obb;

			for (auto itB = std::next(itA); itB != colliders.end(); ++itB)
			{
				const auto& obbB = colliders.get<DRAW::OBB>(*itB).obb;

				GW::I::GCollisionInterface::GCollisionCheck result;
				GW::MATH::GCollision::TestOBBToOBBF(obbA, obbB, result);
				if (result != GW::I::GCollisionInterface::GCollisionCheck::COLLISION)
					continue;

				HandlePair(*itA, *itB);
				HandlePair(*itB, *itA);
			}
		}
	}

	void UpdateBullet(entt::registry& registry)
	{
		auto dt = registry.ctx().get<UTIL::DeltaTime>().dtSec;

		for (auto [entity, bullet] : registry.view<Bullet>().each())
		{
			bullet.lifetime -= dt;
			if (bullet.lifetime <= 0.0f)
				registry.emplace_or_replace<Destroy>(entity);
		}
	}

	void UpdateHit(entt::registry& registry)
	{
		entt::basic_view hit = registry.view<Hit>();
		registry.patch<StateManager>(registry.view<StateManager>().front());
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
			registry.emplace_or_replace<GAME::GameOver>(registry.view<GAME::GameManager>().front(), GAME::GameOver{});
			/// Debug
			std::cout << "You lose, game over." << std::endl;
			///

			AUDIO::AudioSystem::PlayMusicTrack("lose");
		}
	}

	void UpdateDestroy(entt::registry& registry)
	{
		entt::basic_view destroy = registry.view<Destroy>();
		for (auto ent : destroy)
		{
			registry.destroy(ent);
		}

	}

	void Update(entt::registry& registry, entt::entity entity)
	{
#ifdef _DEBUG                     // dump once, in debug builds only
		static bool dumped = false;
		if (!dumped) {
			auto& mm = registry.ctx().get<DRAW::ModelManager>();
			for (auto& [name, col] : mm.models)
				std::cout << "[ModelManager] " << name
				<< "  meshes=" << col.entities.size() << '\n';
			dumped = true;
		}
#endif

		if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front()))
		{
			//RefreshBoundsFromWindow(registry);
			UpdatePosition(registry);
			UpdateClampToScreen(registry);
			UpdateBullet(registry);
			UpdateCollide(registry);
			UpdateHit(registry);
			UpdatePlayers(registry, entity);
			UpdateDestroy(registry);
		}
		else if (!registry.any_of<UI::WinLoseScreen>(registry.view<UI::UIManager>().front()))
			registry.emplace_or_replace<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_update<GameManager>().connect<Update>();
	};
}