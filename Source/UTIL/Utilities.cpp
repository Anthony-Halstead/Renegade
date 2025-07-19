#include "Utilities.h"
#include "../CCL.h"
#include "../DRAW/DrawComponents.h"
#include "../GAME/GameComponents.h"
#include <random>
#include "../APP/Window.hpp"

namespace UTIL
{
	void LookAtMatrix(const GW::MATH::GVECTORF& position, const GW::MATH::GVECTORF& direction, 
		GW::MATH::GMATRIXF& outMatrix, const GW::MATH::GVECTORF& up)
	{
		using namespace GW::MATH;

		// Compute right vector
		GVECTORF right;
		GVector::CrossVector3F(up, direction, right);
		GVector::NormalizeF(right, right);

		// Recompute up to ensure orthogonality
		GVECTORF upOrtho;
		GVector::CrossVector3F(direction, right, upOrtho);
		GVector::NormalizeF(upOrtho, upOrtho);

		// Fill rotation part
		outMatrix = GIdentityMatrixF;
		outMatrix.row1.x = right.x;   outMatrix.row1.y = right.y;   outMatrix.row1.z = right.z;
		outMatrix.row2.x = upOrtho.x; outMatrix.row2.y = upOrtho.y; outMatrix.row2.z = upOrtho.z;
		outMatrix.row3.x = direction.x; outMatrix.row3.y = direction.y; outMatrix.row3.z = direction.z;

		// Set translation
		outMatrix.row4 = position;
	}

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
	bool MoveTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetPosition, float step)
	{
		GW::MATH::GVECTORF& currentPosition = transform.matrix.row4;

		GW::MATH::GVECTORF delta;
		GW::MATH::GVector::SubtractVectorF(targetPosition, currentPosition, delta);

		float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

		if (dist <= step) {
			currentPosition = targetPosition;
			return true;
		}

		GW::MATH::GVECTORF moveStep;
		GW::MATH::GVector::ScaleF(delta, step / dist, moveStep);
		GW::MATH::GVector::AddVectorF(currentPosition, moveStep, currentPosition);

		return false;
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
} // namespace UTIL