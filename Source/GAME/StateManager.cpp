#include "../SB/scoreboard.hpp"
#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../AUDIO/AudioSystem.h"
#include "../UI/UIComponents.h"
#include "../CCL.h"

namespace GAME
{
	// Helper functions

	void GetHighScores(entt::registry& registry) 
	{
		PastScores scoreBoard;
		std::string uuid = "ab417dfa-142f-41af-a81a-dc91a1fcc8b5";

		SB::Client sbClient(uuid);

		if (!sbClient.GetScores(scoreBoard.scores)) 
		{
			std::cout << "SCORE RETRIEVAL ERROR" << std::endl;
		}

		entt::basic_view stateManager = registry.view<StateManager>();
		if (!stateManager.empty())
		{
			registry.emplace_or_replace<PastScores>(stateManager.front(), scoreBoard);

			if (registry.any_of<Score>(stateManager.front()))
			{
				auto& scoreComponent = registry.get<Score>(stateManager.front());
				scoreComponent.highScore = (unsigned)scoreBoard.scores[0];
			}

			registry.emplace<ScoreBoardAquired>(stateManager.front());
		}
	}

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

	void SaveHighScores(entt::registry& registry) 
	{
		entt::basic_view stateManager = registry.view<StateManager>();
		if (stateManager.empty()) return;
		SB::Client sbClient("ab417dfa-142f-41af-a81a-dc91a1fcc8b5");
		if (registry.any_of<Score>(stateManager.front()))
		{
			auto& scoreComponent = registry.get<Score>(stateManager.front());
			auto& pastScores = registry.get<PastScores>(stateManager.front());

			for (int i = 0; i < SB::NUM_SCORES; ++i) {
				if (pastScores.scores[i] < scoreComponent.score) {
					// Shift scores down
					for (int j = SB::NUM_SCORES - 1; j > i; --j) {
						pastScores.scores[j] = pastScores.scores[j - 1];
					}
					pastScores.scores[i] = scoreComponent.score;
					break; // Exit loop after inserting the new score
				}
			}
			if (!sbClient.SaveScores(pastScores.scores)) {
				std::cout << "SCORE SAVE ERROR" << std::endl;
			}
		}
		else {
			std::cout << "No Score component found to save high scores." << std::endl;
		}
	}

	void DisplayFinalScores(entt::registry& registry)
	{
		entt::basic_view scoreManager = registry.view<Score>();
		for (auto ent : scoreManager)
		{
			if (!registry.any_of<ScoreDisplayed>(ent)) {
				auto& scoreComponent = registry.get<Score>(ent);
				/// Debug
				std::cout << "Final Score: " << scoreComponent.score << std::endl;
				std::cout << "High Score: " << scoreComponent.highScore << std::endl;
				///
				SaveHighScores(registry);
				registry.emplace<ScoreDisplayed>(ent);
			}
		}
	}

	// Component functions

	// Provide an object name and a score location and this will update the score according to the data it finds
	void ScoreEvent(entt::registry& registry, const std::string name, const std::string loc)
	{
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		unsigned score = (*config).at(name).at(loc).as<unsigned>();

		entt::basic_view scoreManager = registry.view<Score>();
		for (auto ent : scoreManager)
		{
			UpdateScore(registry, ent, score);
			//DisplayScores(registry, std::to_string(score), std::to_string(registry.get<Score>(ent).highScore));
			/*/// Debug
			 std::cout << "Score updated by: " << score << " from entity: " << name << std::endl;
			 std::cout << "Current Score: " << registry.get<Score>(ent).score << std::endl;
			///*/
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
				/// Debug
				std::cout << "Player has been hit! Current health: " << registry.get<Health>(ent).health << " down from: " << registry.get<PriorFrameData>(ent).pHealth << std::endl;
				///
			}
			// update prior frame data to reflect current frame
			registry.get<PriorFrameData>(ent).pHealth = registry.get<Health>(ent).health;
		}
	}

	// See MonitorPlayerHealth but replace "player" with "boss" in ur head
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
					///// Debug
					// std::cout << "Boss has been hit! Current health: " << registry.get<Health>(ent).health << " down from: " << registry.get<PriorFrameData>(ent).pHealth << std::endl;
					/////
				}
				else {
					/// Debug
					std::cout << "Boss has been hit, but no name was provided, no score awarded " << std::endl;
					///
				}
			}
			if (registry.get<Health>(ent).health <= 0) {
				// communicate to UI or other systems that boss has been defeated
				if (registry.any_of<BossTitle>(ent)) {
					std::string bossName = registry.get<BossTitle>(ent).name;
					ScoreEvent(registry, bossName, "score");
				}
				else {
					/// Debug
					std::cout << "Boss defeated, but no name was provided, no score awarded " << std::endl;
					///
				}
			}
			// update prior frame data to reflect current frame
			registry.get<PriorFrameData>(ent).pHealth = registry.get<Health>(ent).health;
		}
	}

	// See MonitorPlayerHealth but replace "player" with "enemy" in ur head
	void MonitorEnemyHealth(entt::registry& registry)
	{
		entt::basic_view enemies = registry.view<Enemy, Health>();
		for (auto ent : enemies)
		{
			if (registry.get<Health>(ent).health <= 0) 
			{
				if (registry.any_of</*EnemyTitle*/>(ent))
				{
					// std::string enemyName = registry.get<EnemyTitle>(ent).name; // Assuming EnemyTitle component exists
					// ScoreEvent(registry, enemyName, "score");
				}
				else {
					/// Debug
					std::cout << "Enemy defeated, but no name was provided, no score awarded " << std::endl;
					///
				}
			}
		}
	}

	// Update method
	static void Update_GameState(entt::registry& registry, entt::entity entity) 
	{
		if (registry.any_of<UI::TitleScreen>(registry.view<UI::UIManager>().front()) 
			&& !registry.any_of<ScoreBoardAquired>(registry.view<StateManager>().front()))
		{
			GetHighScores(registry);
		}
		else if (!registry.any_of<GAME::GameOver>(registry.view<GAME::GameManager>().front()))
		{
			MonitorPlayerHealth(registry);
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