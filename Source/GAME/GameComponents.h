#ifndef GAME_COMPONENTS_H_
#define GAME_COMPONENTS_H_

namespace GAME
{
	///*** Tags ***///
	struct Player {};
	struct Enemy_Boss {};
	struct Enemy {};
	struct Bullet { float lifetime = 5.0f; };
	struct Collidable {};
	struct Obstacle {};
	struct Destroy {};
	struct Hit {};
	struct GameOver {};
	struct ScoreDisplayed {};
	struct ScoreBoardAquired {};
	struct Gaming {}; // used to show active game session to allow for spawning of new bosses more easily

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
		float spawnTimer = 5.f;
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

	struct PastScores {
		int scores[10];
	};

	struct Shield
	{
		unsigned hitsLeft = 2;				// absorbs this many hits before vanishing
		entt::entity visual = entt::null;	// id of the rendered shield (optional)
	};

}// namespace GAME
#endif // !GAME_COMPONENTS_H_