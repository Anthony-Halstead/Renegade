#ifndef GAME_COMPONENTS_H_
#define GAME_COMPONENTS_H_

namespace GAME
{
	///*** Tags ***///
	struct Player {};
	struct Enemy_Boss {};
	struct Enemy {};
	struct Bullet {};
	struct Collidable {};
	struct Obstacle {};
	struct Destroy {};
	struct Hit {};
	struct GameOver {};

	///*** Components ***///
	struct GameManager {};
	
	struct Transform {
		GW::MATH::GMATRIXF matrix;
	};

	struct Firing {
		float cooldown;
	};

	struct Velocity {
		GW::MATH::GVECTORF vec;
	};

	struct Health {
		unsigned health;
	};

	struct Shatters {
		unsigned shatterCount;
	};

	struct Invulnerability {
		float invulnPeriod;
	};

}// namespace GAME
#endif // !GAME_COMPONENTS_H_