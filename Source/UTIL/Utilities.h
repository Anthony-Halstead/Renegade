#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "GameConfig.h"
#include "../DRAW/DrawComponents.h"
#include "../GAME/GameComponents.h"



namespace UTIL
{
	struct Config
	{
		std::shared_ptr<GameConfig> gameConfig = std::make_shared<GameConfig>();
	};

	struct DeltaTime
	{
		double dtSec;
	};

	struct Input
	{
		GW::INPUT::GController gamePads; // controller support
		GW::INPUT::GInput immediateInput; // twitch keyboard/mouse
		GW::INPUT::GBufferedInput bufferedInput; // event keyboard/mouse

		int connectedControllers = 0; // Controller index
	};

	/// Method declarations

	void CreateVelocity(entt::registry& registry, entt::entity& entity, GW::MATH::GVECTORF& vec, const float& speed);
	void CreateTransform(entt::registry& registry, entt::entity& entity, GW::MATH::GMATRIXF matrix = GW::MATH::GIdentityMatrixF);
	void CreateDynamicObjects(entt::registry& registry, entt::entity& entity, const std::string& model);

	/// Creates a normalized vector pointing in a random direction on the X/Z plane
	GW::MATH::GVECTORF GetRandomVelocityVector();

	GW::MATH::GVECTORF RandomPointInWindowXZ(entt::registry& registry, float marginOffset = 30.f);

	float Distance(const GW::MATH::GVECTORF& a, const GW::MATH::GVECTORF& b);

	bool RotateTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetPosition, float step);

	bool MoveTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetPosition, float step);

	bool ScaleTowards(GAME::Transform& transform, const GW::MATH::GVECTORF& targetScale, float step);
	void Scale(GAME::Transform& transform, float scale);
	GW::MATH::GOBBF BuildOBB(const GW::MATH::GOBBF& local, const GAME::Transform& T);

	void LookAtMatrix(const GW::MATH::GVECTORF& position, const GW::MATH::GVECTORF& direction, GW::MATH::GMATRIXF& outMatrix, const GW::MATH::GVECTORF& up = { 0,1,0,0 });

} // namespace UTIL
#endif // !UTILITIES_H_