#include "GameComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"

namespace GAME
{
	// Helper functions

	void UpdateScore(entt::registry& registry, entt::entity entity, unsigned score)
	{
		if (registry.any_of<Score>(entity))
		{
			auto& scoreComponent = registry.get<Score>(entity);
			scoreComponent.score += score;
		}
		else
		{
			registry.emplace<Score>(entity, score);
		}
	}

	// Component functions
	void ScoreEvent(entt::registry& registry, entt::entity entity, unsigned score, const std::string name, const std::string loc)
	{
		entt::basic_view players = registry.view<Player, Score>();
		for (auto player : players)
		{
			UpdateScore(registry, player, score);
		}
	}

	// Update method
	static void Update_GameState(entt::registry& registry, entt::entity entity) 
	{
		if (!registry.any_of<GameOver>(entity)) 
		{

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