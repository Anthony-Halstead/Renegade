#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"
#include "../AUDIO/AudioSystem.h"
#include "../APP/Window.hpp"


void Invulnerability(entt::registry& registry, entt::entity& entity, const float& deltaTime);
void Shoot(entt::registry& registry, entt::entity& entity, 
	const float& deltaTime, const float& fireRate, UTIL::Input input);
void Movement(entt::registry& registry, entt::entity& entity, 
	const float& deltaTime, const float& speed, UTIL::Input input);

void Update(entt::registry& registry, entt::entity entity) {
	UTIL::Input input = registry.ctx().get<UTIL::Input>();
	double deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;
	auto& playerStats = (*registry.ctx().get<UTIL::Config>().gameConfig).at("Player");

	float speed = playerStats.at("speed").as<float>();
	float fireRate = playerStats.at("firerate").as<float>();

	if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front())) {
		Movement(registry, entity, deltaTime, speed, input);
		Shoot(registry, entity, deltaTime, fireRate, input);
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

void Shoot(entt::registry& registry, entt::entity& entity, 
	const float& deltaTime, const float& fireRate, UTIL::Input input) {

	GAME::Firing* isFiring = registry.try_get<GAME::Firing>(entity);

	if (isFiring != nullptr)
	{
		isFiring->cooldown -= deltaTime;

		if (isFiring->cooldown <= 0) registry.remove<GAME::Firing>(entity);
	}

	float mouseLeftClickState = 0.0f;
	float controllerRightTriggerState = 0.0f;
	input.immediateInput.GetState(G_BUTTON_LEFT, mouseLeftClickState);
	if (input.connectedControllers > 0)
		input.gamePads.GetState(0, G_RIGHT_SHOULDER_BTN, controllerRightTriggerState);

	if (isFiring == nullptr)
	{
		bool fired = 0;
		GW::MATH::GVECTORF velocity = GW::MATH::GIdentityVectorF;		
				
		if (mouseLeftClickState + controllerRightTriggerState == 1.0f)
		{
			// Get player's forward direction
			const auto& playerTransform = registry.get<GAME::Transform>(entity).matrix;
			GW::MATH::GVECTORF forward = playerTransform.row3;

			velocity = forward;
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

void PlayerRotation(entt::registry& registry, entt::entity& entity, GW::MATH::GMATRIXF& transform, UTIL::Input input)
{
	float mouseX, mouseY;
	input.immediateInput.GetMousePosition(mouseX, mouseY);
	auto winView = registry.view<APP::Window>();
	// Get transform of mouse to use for rotation
	if (winView.begin() != winView.end()) {		
		// Rotate player using controller right stick
		float rightStickX, rightStickY;
		if (input.connectedControllers > 0) {
			input.gamePads.GetState(0, G_RX_AXIS, rightStickX);
			input.gamePads.GetState(0, G_RY_AXIS, rightStickY);
			if (std::abs(rightStickX) > 0.5f || std::abs(rightStickY) > 0.5f) {
				// Build a direction vector from the right stick
				GW::MATH::GVECTORF stickDir = { rightStickX, 0.0f, rightStickY, 0.0f };

				// Normalize direction
				GW::MATH::GVector::NormalizeF(stickDir, stickDir);

				// Build a rotation matrix that faces this direction
				GW::MATH::GMATRIXF rotMatrix;
				GW::MATH::GVECTORF up = { 0.0f, 1.0f, 0.0f, 0.0f };
				GW::MATH::GVECTORF right;
				GW::MATH::GVector::CrossVector3F(up, stickDir, right);
				GW::MATH::GVector::NormalizeF(right, right);
				GW::MATH::GVECTORF trueUp;
				GW::MATH::GVector::CrossVector3F(stickDir, right, trueUp);
				GW::MATH::GVector::NormalizeF(trueUp, trueUp);
				rotMatrix.row1 = { right.x,  right.y,  right.z,  0 };
				rotMatrix.row2 = { trueUp.x, trueUp.y, trueUp.z, 0 };
				rotMatrix.row3 = { stickDir.x, stickDir.y, stickDir.z, 0 };
				rotMatrix.row4 = transform.row4;

				// Copy rotation part to player's transform
				transform.row1 = rotMatrix.row1;
				transform.row2 = rotMatrix.row2;
				transform.row3 = rotMatrix.row3;
			}
		}
		else
		{
			//Rotate using mouse direction
			const APP::Window& window = winView.get<APP::Window>(*winView.begin());

			float centeredMouseX = mouseX - (window.width / 2.0f);
			float centeredMouseY = mouseY - (window.height / 2.0f);

			// Create screen-to-world direction
			GW::MATH::GVECTORF targetPosition = {
				-centeredMouseX,
				0.0f,
				centeredMouseY,
				0.0f
			};

			// REPLACE! ONLY FOR TESTING
			constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
			// Apply smoothing
			const float maxStepRad = 4.0f * DEG_TO_RAD; // ~4 degrees per frame

			// Trying to prevent flipping of player if mouse gets too close
			float dx = targetPosition.x - transform.row4.x;
			float dz = targetPosition.z - transform.row4.z;
			float distanceSq = dx * dx + dz * dz;

			if (distanceSq > 10.0f)
			{
				UTIL::RotateTowards(registry.get<GAME::Transform>(entity), targetPosition, maxStepRad);
			}
		}
	}
}

void Movement(entt::registry& registry, entt::entity& entity, 
	const float& deltaTime, const float& speed, UTIL::Input input) {

	GW::MATH::GMATRIXF& transform = registry.get<GAME::Transform>(entity).matrix;
	GW::MATH::GVECTORF movement = {};

	float wKeyState, aKeyState, sKeyState, dKeyState; //keyboard
	float leftStickStateX, leftStickStateY; //controller

	// WASD Movement
	input.immediateInput.GetState(G_KEY_W, wKeyState);
	input.immediateInput.GetState(G_KEY_A, aKeyState);
	input.immediateInput.GetState(G_KEY_S, sKeyState);
	input.immediateInput.GetState(G_KEY_D, dKeyState);

	// Controller Movement
	if (input.connectedControllers > 0) {
		input.gamePads.GetState(0, G_LX_AXIS, leftStickStateX);
		input.gamePads.GetState(0, G_LY_AXIS, leftStickStateY);
	}
	else {
		// No controller connected, set controller-specific inputs to zero
		leftStickStateX = 0.0f;
		leftStickStateY = 0.0f;
	}

	if (wKeyState == 1.0f || leftStickStateY > 0.5f) movement.z += 1;
	if (aKeyState == 1.0f || leftStickStateX < -0.5f) movement.x -= 1;
	if (sKeyState == 1.0f || leftStickStateY < -0.5f) movement.z -= 1;
	if (dKeyState == 1.0f || leftStickStateX > 0.5f) movement.x += 1;

	GW::MATH::GVector::NormalizeF(movement, movement);
	GW::MATH::GVector::ScaleF(movement, speed * deltaTime, movement);
	GW::MATH::GVector::AddVectorF(transform.row4, movement, transform.row4);

	PlayerRotation(registry, entity, transform, input);
}

CONNECT_COMPONENT_LOGIC()
{
	registry.on_update<GAME::Player>().connect<Update>();
};
