#include "GameComponents.h"
#include "../UTIL/Utilities.h"
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
			registry.emplace<Score>(entity, score);
			auto& scoreComponent = registry.get<Score>(entity);
			scoreComponent.highScore = score;
		}
	}

	// Component functions

	// Pre-implementation in the event that we opt for time-based scoring.
	void ScoreEvent(entt::registry& registry, entt::entity entity, const std::string name, const std::string loc)
	{
		// TODO: grab config time-based score increase value, if one exists.
		unsigned score = 0;

		entt::basic_view players = registry.view<Player, Score>();
		for (auto player : players)
		{
			UpdateScore(registry, player, score);
		}
	}

	// This function monitors player health frame-to-frame and updates the prior frame data to reflect the current health.
	void MonitorPlayerHealth(entt::registry& registry, entt::entity entity)
	{
		entt::basic_view players = registry.view<Player, Health, PriorFrameData>();
		for (auto ent : players)
		{
			if (registry.get<Health>(ent).health < registry.get<PriorFrameData>(ent).pHealth) {
				// communicate to UI or other systems that player has been hit
			}
			// update prior frame data to reflect current frame
			registry.get<PriorFrameData>(ent).pHealth = registry.get<Health>(ent).health;
		}
	}

	// Update method
	static void Update_GameState(entt::registry& registry, entt::entity entity) 
	{
		if (!registry.any_of<GameOver>(entity)) 
		{
			MonitorPlayerHealth(registry, entity);
		}
		else
		{

		}
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_update<StateManager>().connect<Update_GameState>();
	}

}