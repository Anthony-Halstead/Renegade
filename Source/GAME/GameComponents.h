#ifndef GAME_COMPONENTS_H_
#define GAME_COMPONENTS_H_

namespace GAME
{
	///*** Tags ***///
	struct Player {};
	struct Enemy_Boss {};
	struct Enemy {};
	struct Bullet { float lifetime = 25.0f; };
	struct Collidable {};
	struct Obstacle {};
	struct Destroy {};
	struct Hit {};
	struct GameOver {};

	struct BoundsManager
	{
		int left, right, bottom, top;
	};
	struct Bounded {};
	///*** Components ***///
	struct GameManager {};

	struct StateManager {};
	
	struct Transform {
		GW::MATH::GMATRIXF matrix;
	};

	struct TargetPosition {
		GW::MATH::GVECTORF position;
	};

	struct EnemyState {
		enum class State {
			Moving,
			Ready,
			Attacking
		} state;
	};


	struct Firing {
		float cooldown;
	};

	struct BulletOwner {
		entt::entity owner;
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

	struct SpawnEnemies {
		float spawnTimer;
	};

	struct Score {
		unsigned score;
		unsigned highScore;
	};

	struct PriorFrameData {
		unsigned pHealth;
	};

	struct BossTitle {
		std::string name;
	};

}// namespace GAME
#endif // !GAME_COMPONENTS_H_