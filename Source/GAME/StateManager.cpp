#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../CCL.h"

namespace GAME
{
	// Helper functions

	// This updates the player score and checks if it is a new high score.
	// If the score component does not exist, it creates one with the initial score.
	// TODO: Possible redundancy, evaluate method during polish and code cleanup.
	void UpdateScore(entt::registry& registry, entt::entity entity, unsigned score)
	{
		if (registry.any_of<Score>(entity))
		{
			auto& scoreComponent = registry.get<Score>(entity);
			scoreComponent.score += score;
			if (scoreComponent.score > scoreComponent.highScore) 
			{
				scoreComponent.highScore = scoreComponent.score;
			}
		}
		else
		{
			// this should never happen, but if it does, we create a new Score component.
			registry.emplace<Score>(entity, score);
			auto& scoreComponent = registry.get<Score>(entity);
			scoreComponent.highScore = score;
		}
	}

	void DisplayFinalScores(entt::registry& registry)
	{
		entt::basic_view scoreManager = registry.view<Score>();
		for (auto ent : scoreManager)
		{
			if (!registry.any_of<ScoreDisplayed>(ent)) {
				auto& scoreComponent = registry.get<Score>(ent);
				std::cout << "Final Score: " << scoreComponent.score << std::endl;
				std::cout << "High Score: " << scoreComponent.highScore << std::endl;
				registry.emplace<ScoreDisplayed>(ent);
			}
		}
	}

	// Component functions

	// Pre-implementation included in the event that we opt for time-based scoring.
	void ScoreEvent(entt::registry& registry, const std::string name, const std::string loc)
	{
		// TODO: grab config time-based score increase value, if one exists.
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		unsigned score = (*config).at(name).at(loc).as<unsigned>();

		entt::basic_view scoreManager = registry.view<Score>();
		for (auto ent : scoreManager)
		{
			UpdateScore(registry, ent, score);
			// std::cout << "Current Score: " << registry.get<Score>(ent).score << std::endl;
		}
	}

	// This function monitors player health frame-to-frame and updates the prior frame data to reflect the current health.
	void MonitorPlayerHealth(entt::registry& registry)
	{
		entt::basic_view players = registry.view<Player, Health, PriorFrameData>();
		for (auto ent : players)
		{
			if (registry.get<Health>(ent).health < registry.get<PriorFrameData>(ent).pHealth) {
				// communicate to UI or other systems that player has been hit
				std::cout << "Player has been hit! Current health: " << registry.get<Health>(ent).health << " down from: " << registry.get<PriorFrameData>(ent).pHealth << std::endl;
			}
			// update prior frame data to reflect current frame
			registry.get<PriorFrameData>(ent).pHealth = registry.get<Health>(ent).health;
		}
	}

	void MonitorBossHealth(entt::registry& registry)
	{
		entt::basic_view bosses = registry.view<Enemy_Boss, Health, PriorFrameData>();
		for (auto ent : bosses)
		{
			if (registry.get<Health>(ent).health < registry.get<PriorFrameData>(ent).pHealth) {
				// communicate to UI or other systems that boss has been hit
				if (registry.any_of<BossTitle>(ent)) {
					std::string bossName = registry.get<BossTitle>(ent).name;
					ScoreEvent(registry, bossName, "scorePerHP");
				}
				else {
					std::cout << "Boss has been hit, but no name was provided, no score awarded " << std::endl;
				}
				// std::cout << "Boss has been hit! Current health: " << registry.get<Health>(ent).health << " down from: " << registry.get<PriorFrameData>(ent).pHealth << std::endl;
			}
			if (registry.get<Health>(ent).health <= 0) {
				// communicate to UI or other systems that boss has been defeated
				if (registry.any_of<BossTitle>(ent)) {
					std::string bossName = registry.get<BossTitle>(ent).name;
					ScoreEvent(registry, bossName, "score");
				}
				else {
					std::cout << "Boss defeated, but no name was provided, no score awarded " << std::endl;
				}
			}
			// update prior frame data to reflect current frame
			registry.get<PriorFrameData>(ent).pHealth = registry.get<Health>(ent).health;
		}

		auto bossesLeft = registry.view<Enemy_Boss>();
		if (bossesLeft.empty()) {
			AUDIO::AudioSystem::PlayMusicTrack("win");
		}
	}

	void MonitorEnemyHealth(entt::registry& registry)
	{
		entt::basic_view enemies = registry.view<Enemy, Health>();
		for (auto ent : enemies)
		{
			if (registry.get<Health>(ent).health <= 0) {
				// communicate to UI or other systems that enemy has been defeated
				ScoreEvent(registry, "Enemy1", "score");
			}
		}
	}

	// Update method
	static void Update_GameState(entt::registry& registry, entt::entity entity) 
	{
		if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front()))
		{
			MonitorPlayerHealth(registry);
			// ScoreEvent(registry, "Player", "score");
			MonitorBossHealth(registry);
			MonitorEnemyHealth(registry);
		}
		else
		{
			DisplayFinalScores(registry);
		}
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_update<StateManager>().connect<Update_GameState>();
	}

}