#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"
#include "../AUDIO/AudioSystem.h"


void Invulnerability(entt::registry& registry, entt::entity& entity, const float& deltaTime);
void Shoot(entt::registry& registry, entt::entity& entity, const float& deltaTime, const float& fireRate);
void Movement(entt::registry& registry, entt::entity& entity, const float& deltaTime, const float& speed);

void Update(entt::registry& registry, entt::entity entity) {
	UTIL::Input input = registry.ctx().get<UTIL::Input>();
	double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;
	auto& playerStats = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Player");

	float speed = playerStats.at("speed").as<float>();
	float fireRate = playerStats.at("firerate").as<float>();

	if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front())) {
		Movement(registry, entity, deltaTime, speed);
		Shoot(registry, entity, deltaTime, fireRate);
		Invulnerability(registry, entity, deltaTime);
	}
}

void Invulnerability(entt::registry& registry, entt::entity& entity, const float& deltaTime) {
	GAME::Invulnerability* isInvul = registry.try_get<GAME::Invulnerability>(entity);

	if (isInvul != nullptr)
	{
		isInvul->invulnPeriod -= deltaTime;

		if (isInvul->invulnPeriod <= 0) registry.remove<GAME::Invulnerability>(entity);
	}
}

void Shoot(entt::registry& registry, entt::entity& entity, const float& deltaTime, const float& fireRate) {

	GAME::Firing* isFiring = registry.try_get<GAME::Firing>(entity);

	if (isFiring != nullptr)
	{
		isFiring->cooldown -= deltaTime;

		if (isFiring->cooldown <= 0) registry.remove<GAME::Firing>(entity);
	}

	if (isFiring == nullptr)
	{
		bool fired = 0;
		GW::MATH::GVECTORF velocity = GW::MATH::GIdentityVectorF;

		if (GetAsyncKeyState(0x26) & 0x8000)
		{
			velocity.z += 1;
			fired = 1;
		}
		else if (GetAsyncKeyState(0x28) & 0x8000)
		{
			velocity.z -= 1;
			fired = 1;
		}
		if (GetAsyncKeyState(0x25) & 0x8000)
		{
			velocity.x -= 1;
			fired = 1;
		}
		else if (GetAsyncKeyState(0x27) & 0x8000)
		{
			velocity.x += 1;
			fired = 1;
		}
		if (fired)
		{
			entt::entity bullet = registry.create();
			registry.emplace<GAME::Bullet>(bullet, 25.0f);
			registry.emplace<GAME::BulletOwner>(bullet, entity);
			std::string bulletModel = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Bullet").at("model").as<std::string>();
			float bulletSpeed = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Bullet").at("speed").as<float>();

			UTIL::CreateVelocity(registry, bullet, velocity, bulletSpeed);
			UTIL::CreateTransform(registry, bullet, registry.get<GAME::Transform>(entity).matrix);
			UTIL::CreateDynamicObjects(registry, bullet, bulletModel);

			GAME::Firing& firing = registry.emplace<GAME::Firing>(entity);
			firing.cooldown = fireRate;

			if (registry.valid(bullet)) {
				if (registry.all_of<GAME::Transform>(bullet)) {
					// Create a 3D SFX instance that follows the bullet
					const auto& bulletTransform = registry.get<GAME::Transform>(bullet).matrix;
					GW::MATH::GVECTORF bulletPos = bulletTransform.row4;

					AUDIO::AudioSystem::PlaySFX("menuClick", bulletPos);
				}
			}

		}
	}
}

void Movement(entt::registry& registry, entt::entity& entity, const float& deltaTime, const float& speed) {

	GW::MATH::GMATRIXF& transform = registry.get<GAME::Transform>(entity).matrix;
	GW::MATH::GVECTORF movement = {};

	// WASD
	if (GetAsyncKeyState(0x57) & 0x8000) movement.z += 1;
	if (GetAsyncKeyState(0x41) & 0x8000) movement.x -= 1;
	if (GetAsyncKeyState(0x53) & 0x8000) movement.z -= 1;
	if (GetAsyncKeyState(0x44) & 0x8000) movement.x += 1;

	GW::MATH::GVector::NormalizeF(movement, movement);
	GW::MATH::GVector::ScaleF(movement, speed * deltaTime, movement);
	GW::MATH::GVector::AddVectorF(transform.row4, movement, transform.row4);
}

CONNECT_COMPONENT_LOGIC()
{
	registry.on_update<GAME::Player>().connect<Update>();
};
