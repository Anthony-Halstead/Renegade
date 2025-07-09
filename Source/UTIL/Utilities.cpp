#include "Utilities.h"
#include "../CCL.h"
#include "../DRAW/DrawComponents.h"
#include "../GAME/GameComponents.h"

namespace UTIL
{
	GW::MATH::GVECTORF GetRandomVelocityVector()
	{
		GW::MATH::GVECTORF vel = { float((rand() % 20) - 10), 0.0f, float((rand() % 20) - 10) };
		if (vel.x <= 0.0f && vel.x > -1.0f) vel.x = -1.0f;
		else if (vel.x >= 0.0f && vel.x < 1.0f) vel.x = 1.0f;

		if (vel.z <= 0.0f && vel.z > -1.0f) vel.z = -1.0f;
		else if (vel.z >= 0.0f && vel.z < 1.0f) vel.z = 1.0f;

		GW::MATH::GVector::NormalizeF(vel, vel);

		return vel;
	}

	void CreateVelocity(entt::registry& registry, entt::entity& entity, GW::MATH::GVECTORF& vec, const float& speed)
	{
		GW::MATH::GVector::NormalizeF(vec, vec);
		GW::MATH::GVector::ScaleF(vec, speed, vec);
		registry.emplace<GAME::Velocity>(entity, vec);
	}

	void CreateTransform(entt::registry& registry, entt::entity& entity, GW::MATH::GMATRIXF matrix)
	{
		registry.emplace<GAME::Transform>(entity, matrix);
	}

	void CreateDynamicObjects(entt::registry& registry, entt::entity& entity, const std::string& model)
	{
		registry.emplace<DRAW::MeshCollection>(entity);
		registry.emplace<GAME::Collidable>(entity);

		DRAW::ModelManager& modelManager = registry.ctx().get<DRAW::ModelManager>();
		std::map<std::string, DRAW::MeshCollection>::iterator it = modelManager.models.find(model);

		unsigned meshCount = 0;

		if (it != modelManager.models.end())
		{
			for (entt::entity& ent : it->second.entities)
			{
				entt::entity meshEntity = registry.create();

				registry.emplace<DRAW::GPUInstance>(meshEntity, registry.get<DRAW::GPUInstance>(ent));
				registry.emplace<DRAW::GeometryData>(meshEntity, registry.get<DRAW::GeometryData>(ent));

				registry.get<DRAW::MeshCollection>(entity).entities.push_back(meshEntity);
				registry.get<DRAW::MeshCollection>(entity).obb = it->second.obb;
			}
		}

		GAME::Transform* transform = registry.try_get<GAME::Transform>(entity);

		if (transform == nullptr)
		{
			registry.emplace<GAME::Transform>(entity);
			registry.get<GAME::Transform>(entity).matrix = registry.get<DRAW::GPUInstance>(registry.get<DRAW::MeshCollection>(entity).entities[0]).transform;
		}
	}

} // namespace UTIL