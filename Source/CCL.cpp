#include "CCL.h"

namespace
{
	// Component logic functions
	std::list<std::function<void(entt::registry& reg)>> componentLogic;

	std::list<std::function<void(entt::registry& reg)>>& GetComponentLogic()
	{
		return componentLogic;
	}
}

namespace CCL {
	

	ComponentLogic::ComponentLogic(std::function<void(entt::registry& reg)> logic){
		GetComponentLogic().push_back(logic);
	}

	void InitializeComponentLogic(entt::registry& registry){
		for (auto& logic : GetComponentLogic()) {
			logic(registry);
		}
	}
}