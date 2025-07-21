#include "Utilities.h"
#include "../CCL.h"
#include "../DRAW/DrawComponents.h"
#include "../GAME/GameComponents.h"
#include <random>
#include "../APP/Window.hpp"

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

		auto& T = registry.get<GAME::Transform>(entity);

		const GW::MATH::GOBBF& localBox = it->second.obb;
		registry.emplace<DRAW::OBB>(entity, DRAW::OBB{ UTIL::BuildOBB(localBox, T) });
	}
	GW::MATH::GVECTORF RandomPointInWindowXZ(entt::registry& registry, float marginOffset)
	{
		using namespace GW::MATH;
		auto wView = registry.view<APP::Window>();
		const auto& window = wView.get<APP::Window>(*wView.begin());

		float halfWidth = static_cast<float>(window.width) / marginOffset;
		float halfHeight = static_cast<float>(window.height) / marginOffset;

		float minX = -halfWidth;
		float maxX = halfWidth;
		float minZ = -halfHeight;
		float maxZ = halfHeight;

		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> dx(minX, maxX);
		std::uniform_real_distribution<float> dz(minZ, maxZ);

		return GVECTORF{ dx(rng), 0.f, dz(rng), 0.f };
	}

	float Distance(const GW::MATH::GVECTORF& a, const GW::MATH::GVECTORF& b)
	{
		GW::MATH::GVECTORF delta;
		GW::MATH::GVector::SubtractVectorF(b, a, delta);
		return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
	}

	bool RotateTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetPosition, float maxStepRad)
	{
		using namespace GW::MATH;
		const GVECTORF upAxis = { 0,1,0,0 };

		GVECTORF desired;
		GVector::SubtractVectorF(targetPosition, transform.matrix.row4, desired);
		desired.y = 0;
		GVector::NormalizeF(desired, desired);

		GVECTORF forward = transform.matrix.row3;
		forward.y = 0;
		GVector::NormalizeF(forward, forward);

		float dot; GVector::DotF(forward, desired, dot);
		dot = std::clamp(dot, -1.f, 1.f);
		float angle = std::acos(dot);

		bool reached = false;

		if (angle < 1e-4f) {
			forward = desired;
			reached = true;
		}

		else if (dot < -0.9999f) {

			GQUATERNIONF q;
			GQuaternion::SetByVectorAngleF(upAxis, maxStepRad, q);
			GMATRIXF m; GMatrix::ConvertQuaternionF(q, m);

			GVECTORF out; GVector::TransformF(forward, m, out);
			GVector::NormalizeF(out, forward);

			reached = (maxStepRad >= angle - 1e-4f);
		}
		else {
			GVECTORF cross; GVector::CrossVector3F(forward, desired, cross);
			float dir = (cross.y >= 0.f) ? 1.f : -1.f;

			float step = min(angle, maxStepRad) * dir;
			GQUATERNIONF q;
			GQuaternion::SetByVectorAngleF(upAxis, step, q);
			GMATRIXF m;
			GMatrix::ConvertQuaternionF(q, m);

			GVECTORF out;
			GVector::TransformF(forward, m, out);
			GVector::NormalizeF(out, forward);

			if (std::abs(angle) <= maxStepRad + 1e-4f)
			{
				forward = desired;
				reached = true;
			}
		}

		GVECTORF right, trueUp;
		GVector::CrossVector3F(upAxis, forward, right);
		GVector::NormalizeF(right, right);
		GVector::CrossVector3F(forward, right, trueUp);
		GVector::NormalizeF(trueUp, trueUp);

		auto& M = transform.matrix;
		M.row1 = { right.x,  right.y,  right.z,  0 };
		M.row2 = { trueUp.x, trueUp.y, trueUp.z, 0 };
		M.row3 = { forward.x, forward.y, forward.z, 0 };

		return reached;
	}

	bool SteerTowards(GAME::Velocity& vel,
		const GW::MATH::GVECTORF& from,
		const GW::MATH::GVECTORF& to,
		float maxSpeed,
		float arrivalDist)
	{
		using namespace GW::MATH;

		GVECTORF delta{ to.x - from.x, 0, to.z - from.z, 0 };
		float d2 = delta.x * delta.x + delta.z * delta.z;

		if (d2 <= arrivalDist * arrivalDist)
		{
			vel.vec = { 0,0,0,0 };
			return true;
		}

		float d = std::sqrt(d2);
		GVECTORF dir;
		GVector::ScaleF(delta, 1.f / d, dir);
		GVector::ScaleF(dir, maxSpeed, vel.vec);
		return false;
	}

	bool MoveTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetPosition, float step)
	{
		using namespace GW::MATH;

		GVECTORF& currentPosition = transform.matrix.row4;

		// Calculate delta only in the XZ plane
		GVECTORF delta;
		delta.x = targetPosition.x - currentPosition.x;
		delta.y = 0.0f; // force flat movement
		delta.z = targetPosition.z - currentPosition.z;
		delta.w = 0.0f;

		// Calculate distance in XZ plane only
		float dist = std::sqrt(delta.x * delta.x + delta.z * delta.z);

		if (dist <= step) {
			currentPosition.x = targetPosition.x;
			currentPosition.z = targetPosition.z;
			currentPosition.y = 0.0f; // lock to XZ plane
			return true;
		}

		// Scale step and move
		GVECTORF moveStep;
		GVector::ScaleF(delta, step / dist, moveStep);
		GVector::AddVectorF(currentPosition, moveStep, currentPosition);

		// Enforce XZ plane constraint
		currentPosition.y = 0.0f;

		return false;
	}
	void RotateContinuous(GAME::Transform& transform, float step, char axis)
	{
		using namespace GW::MATH;
		switch (axis)
		{
		case 'X': GMatrix::RotateXLocalF(transform.matrix, step, transform.matrix); break;
		case 'Y': GMatrix::RotateYLocalF(transform.matrix, step, transform.matrix); break;
		case 'Z': GMatrix::RotateZLocalF(transform.matrix, step, transform.matrix); break;
		default: break;
		}
	}
	bool ScaleTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetScale, float step)
	{
		using namespace GW::MATH;

		GVECTORF currentScale;
		GMatrix::GetScaleF(transform.matrix, currentScale);

		GVECTORF diff;
		GVector::SubtractVectorF(targetScale, currentScale, diff);
		float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

		bool reached = false;
		GVECTORF newScale;
		if (dist <= step || dist < 1e-6f) {
			newScale = targetScale;
			reached = true;
		}
		else {
			GVECTORF dir;
			GVector::ScaleF(diff, 1.f / dist, dir);
			GVector::ScaleF(dir, step, dir);
			GVector::AddVectorF(currentScale, dir, newScale);
		}

		GVECTORF ratio = {
			(std::fabs(currentScale.x) > 1e-6f ? newScale.x / currentScale.x : newScale.x),
			(std::fabs(currentScale.y) > 1e-6f ? newScale.y / currentScale.y : newScale.y),
			(std::fabs(currentScale.z) > 1e-6f ? newScale.z / currentScale.z : newScale.z),
			0
		};

		GMatrix::ScaleGlobalF(transform.matrix, ratio, transform.matrix);

		return reached;
	}
	void Scale(GAME::Transform& transform, float scale)
	{
		GW::MATH::GMatrix::ScaleGlobalF(transform.matrix, GW::MATH::GVECTORF{ scale, scale, scale, 0 }, transform.matrix);
	}
	GW::MATH::GOBBF BuildOBB(const GW::MATH::GOBBF& local, const GAME::Transform& T)
	{
		using namespace GW::MATH;
		GOBBF w = local;
		GVECTORF s;  GMatrix::GetScaleF(T.matrix, s);
		w.extent.x *= s.x;
		w.extent.y *= s.y;
		w.extent.z *= s.z;

		GQUATERNIONF entityRot;
		GQuaternion::SetByMatrixF(T.matrix, entityRot);
		GQuaternion::MultiplyQuaternionF(local.rotation, entityRot, w.rotation);

		w.center = T.matrix.row4;

		return w;
	}

	void StretchModel(entt::registry& registry, GAME::Transform& transform, GW::MATH::GVECTORF startPoint,
		GW::MATH::GVECTORF endPoint, GW::MATH::GVECTORF scaleValue, char axis)
	{
		using namespace GW::MATH;
		GVECTORF direction;
		GVector::SubtractVectorF(endPoint, transform.matrix.row4, direction);

		float length = 0.0f;
		/*	GVECTORF dotProduct;*/
		GVector::DotF(direction, direction, length);

		//length = dotProduct.x; // since direction is a 3D vector, we can use any component for length
		if (length < 1e-6f) return; // Avoid division by zero
		length = std::sqrt(length);

		// Normalize direction
		GVECTORF normalizedDirection;
		GVector::NormalizeF(direction, normalizedDirection);

		// Midpoint for positioning
		GVECTORF midpoint;
		GVector::AddVectorF(startPoint, endPoint, midpoint);
		GMatrix::ScaleLocalF(transform.matrix, midpoint, transform.matrix);

		GVECTORF position;
		GVector::AddVectorF(startPoint, endPoint, position);

		// Set scale based on stretch axis
		GVECTORF scale = { 1.0f, 1.0f, 1.0f, 0.0f };
		switch (axis)
		{
		case 'X':
			scale.x = length;
			break;
		case 'Y':
			scale.y = length;
			break;
		case 'Z':
			scale.z = length;
			break;
		default:
			scale.x = length; // Default to X if no valid axis is provided
			break;
		}
	}
} // namespace UTIL