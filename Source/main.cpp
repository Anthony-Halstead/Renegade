// main entry point for the application
// enables components to define their behaviors locally in an .hpp file
#include "CCL.h"
#include "UTIL/Utilities.h"
// include all components, tags, and systems used by this program
#include "GAME/ItemPickupComponents.h"
#include "GAME/WeaponComponents.h"
#include "GAME/ShieldComponents.h"
#include "DRAW/DrawComponents.h"
#include "GAME/GameComponents.h"
#include "APP/Window.hpp"
#include "AUDIO/AudioComponents.h"
#include "AUDIO/AudioSystem.h"
#include "AI/AIComponents.h"
#include "UI/UIComponents.h"

// Local routines for specific application behavior
void GraphicsBehavior(entt::registry& registry);
void GameplayBehavior(entt::registry& registry);
void AudioBehavior(entt::registry& registry);
void AIBehavior(entt::registry& registry);
void UIBehavior(entt::registry& registry);
void PickupBehavior(entt::registry& registry);
void MainLoopBehavior(entt::registry& registry);

enum class AppState { Splash, MainMenu, Settings, Credits, Scores, GameLoop };

// Architecture is based on components/entities pushing updates to other components/entities (via "patch" function)
int main()
{

	// All components, tags, and systems are stored in a single registry
	entt::registry registry;

	entt::locator<entt::registry>::reset(&registry, [](entt::registry*) {});

	// initialize the ECS Component Logic
	CCL::InitializeComponentLogic(registry);

	// Seed the rand
	unsigned int time = std::chrono::steady_clock::now().time_since_epoch().count();
	srand(time);

	registry.ctx().emplace<UTIL::Config>();
	registry.ctx().emplace<DRAW::ModelManager>();

	GraphicsBehavior(registry); // create windows, surfaces, and renderers
	AudioBehavior(registry); //Create audio manager

	GameplayBehavior(registry); // create entities and components for gameplay
	AIBehavior(registry); //Create AI Director
	PickupBehavior(registry);
	UIBehavior(registry); // Create UI 

	MainLoopBehavior(registry); // update windows and input

	// clear all entities and components from the registry
	// invokes on_destroy() for all components that have it
	// registry will still be intact while this is happening
	registry.clear();

	return 0; // now destructors will be called for all components
}

void AudioBehavior(entt::registry& registry)
{
	auto audioEnt = registry.create();
	registry.emplace<AUDIO::AudioDevice>(audioEnt);

	AUDIO::AudioSystem::Initialize(&registry, audioEnt);
}

void AIBehavior(entt::registry& registry)
{
	auto e = registry.create();
	registry.emplace<AI::AIDirector>(e);
}

void PickupBehavior(entt::registry& registry)
{
	auto ent = registry.create();
	registry.emplace<GAME::PickupManager>(ent);

	/* weapon manager */
	auto weaponEnt = registry.create();
	registry.emplace<GAME::WeaponManager>(weaponEnt);

	// in main.cpp inside PickupBehavior() – same place you made WeaponManager
	auto shEnt = registry.create();
	registry.emplace<GAME::ShieldManager>(shEnt);

}

void UIBehavior(entt::registry& registry)
{
	auto winView = registry.view<GW::SYSTEM::GWindow>();
	entt::entity window = *winView.begin();

	registry.emplace<UI::UIManager>(window);
}
// This function will be called by the main loop to update the graphics
// It will be responsible for loading the Level, creating the VulkanRenderer, and all VulkanInstances
void GraphicsBehavior(entt::registry& registry)
{
	std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

	// Add an entity to handle all the graphics data
	auto display = registry.create();

	registry.emplace<DRAW::CPULevel>(display, DRAW::CPULevel{
		(*config).at("Level1").at("levelFile").as<std::string>().c_str(),
		(*config).at("Level1").at("modelPath").as<std::string>().c_str()
		});

	// Emplace and initialize Window component
	int windowWidth = (*config).at("Window").at("width").as<int>();
	int windowHeight = (*config).at("Window").at("height").as<int>();
	int startX = (*config).at("Window").at("xstart").as<int>();
	int startY = (*config).at("Window").at("ystart").as<int>();
	registry.emplace<APP::Window>(display, APP::Window{
		startX,
		startY,
		windowWidth,
		windowHeight,
		GW::SYSTEM::GWindowStyle::WINDOWEDLOCKED, "Renegade"
		});


	// Create the input
	auto& input = registry.ctx().emplace<UTIL::Input>();
	auto& window = registry.get<GW::SYSTEM::GWindow>(display);
	input.bufferedInput.Create(window);
	input.immediateInput.Create(window);
	input.gamePads.Create();
	auto& pressEvents = registry.ctx().emplace<GW::CORE::GEventCache>();
	pressEvents.Create(32);
	input.bufferedInput.Register(pressEvents);
	input.gamePads.Register(pressEvents);
	input.gamePads.GetNumConnected(input.connectedControllers);

	// Create a transient component to initialize the Renderer
	std::string vertShader = (*config).at("Shaders").at("vertex").as<std::string>();
	std::string pixelShader = (*config).at("Shaders").at("pixel").as<std::string>();
	registry.emplace<DRAW::VulkanRendererInitialization>(display,
		DRAW::VulkanRendererInitialization{
			vertShader, pixelShader,
			{ {0.2f, 0.2f, 0.25f, 1} } , { 1.0f, 0u }, 75.f, 0.1f, 100.0f });


	registry.emplace<DRAW::VulkanRenderer>(display);

	registry.emplace<DRAW::GPULevel>(display);

	// Register for Vulkan clean up
	GW::CORE::GEventResponder shutdown;
	shutdown.Create([&](const GW::GEvent& e)
		{
			GW::GRAPHICS::GVulkanSurface::Events event;
			GW::GRAPHICS::GVulkanSurface::EVENT_DATA data;
			if (+e.Read(event, data) && event == GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES) registry.clear<DRAW::VulkanRenderer>();
		});
	registry.get<DRAW::VulkanRenderer>(display).vlkSurface.Register(shutdown);
	registry.emplace<GW::CORE::GEventResponder>(display, shutdown.Relinquish());

	// Create a camera and emplace it
	GW::MATH::GMATRIXF initialCamera;
	GW::MATH::GVECTORF translate = { 0.0f,45.0f, 0.0f };
	GW::MATH::GVECTORF lookat = { 0.0f, 0.0f, 0.0f };
	GW::MATH::GVECTORF up = { 0.0f, 0.0f, 1.0f };
	GW::MATH::GMatrix::TranslateGlobalF(initialCamera, translate, initialCamera);
	GW::MATH::GMatrix::LookAtLHF(translate, lookat, up, initialCamera);
	// Inverse to turn it into a camera matrix, not a view matrix. This will let us do
	// camera manipulation in the component easier
	GW::MATH::GMatrix::InverseF(initialCamera, initialCamera);
	registry.emplace<DRAW::Camera>(display, DRAW::Camera{ initialCamera });
}

// This function will be called by the main loop to update the gameplay
// It will be responsible for updating the VulkanInstances and any other gameplay components
void GameplayBehavior(entt::registry& registry)
{
	entt::entity player = registry.create();
	entt::entity gameManager = registry.create();
	entt::entity stateManager = registry.create();

	registry.emplace<GAME::GameManager>(gameManager);
	registry.emplace<GAME::StateManager>(stateManager);
	registry.emplace<GAME::Player>(player, 0, 5);

	std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
	std::string playerModel = (*config).at("Player").at("model").as<std::string>();
	unsigned playerHealth = (*config).at("Player").at("hitpoints").as<unsigned>();

	// Move enemy to top of the screen to prepare for ship model
	GAME::Transform playerTransform{};

	GW::MATH::GMATRIXF identity;
	GW::MATH::GMatrix::IdentityF(identity);

	// Put player below enemy to start
	playerTransform.matrix = identity;
	playerTransform.matrix.row4.z = -20.0f;

	// Initilize player direction to face forward
	float mouseX, mouseY;
	auto winView = registry.view<APP::Window>();
	UTIL::Input input;
	input.immediateInput.GetMousePosition(mouseX, mouseY);
	const APP::Window& window = winView.get<APP::Window>(*winView.begin());

	GW::MATH::GVECTORF playerPosition = playerTransform.matrix.row4;
	float centeredMouseX = mouseX - (window.width / 2.0f);
	float centeredMouseY = mouseY - (window.height / 2.0f);

	// Calculate target position in world space
	GW::MATH::GVECTORF targetPosition = {
		playerPosition.x + centeredMouseX,
		0.0f,
		playerPosition.z - centeredMouseY,
		0.0f
	};

	// Instantly rotate to face the mouse
	UTIL::RotateTowards(playerTransform, targetPosition, FLT_MAX);


	registry.emplace<GAME::Health>(player, playerHealth);
	registry.emplace<GAME::Transform>(player, playerTransform);

	registry.emplace<GAME::Bounded>(player);
	registry.emplace<GAME::Gaming>(gameManager);

	registry.emplace<GAME::PriorFrameData>(player, GAME::PriorFrameData{ playerHealth });
	registry.emplace<GAME::Score>(stateManager, GAME::Score{ 0, 0 });
	registry.patch<GAME::StateManager>(stateManager);


	UTIL::CreateDynamicObjects(registry, player, playerModel);
}

// This function will be called by the main loop to update the main loop
// It will be responsible for updating any created windows and handling any input
void MainLoopBehavior(entt::registry& registry)
{
	AppState state = AppState::GameLoop;

	int closedCount;
	bool winCloseFound = false;
	auto winView = registry.view<APP::Window>();
	auto& deltaTime = registry.ctx().emplace<UTIL::DeltaTime>().dtSec;

	std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

	if (!(*config).at("UI").at("disableSplash").as<bool>())
		registry.emplace<UI::SplashScreen>(registry.view<UI::UIManager>().front(), UI::SplashScreen{ (*config).at("UI").at("splashDuration").as<float>(), (*config).at("UI").at("fadeDuration").as<float>() });

	registry.emplace<UI::TitleScreen>(registry.view<UI::UIManager>().front());

	do {
		static auto frameStart = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count();
		frameStart = std::chrono::steady_clock::now();
		if (elapsed > 1.0 / 30.0) elapsed = 1.0 / 30.0;
		deltaTime = elapsed;

		closedCount = 0;
		winCloseFound = false;

		if (!registry.any_of<UI::TitleScreen>(registry.view<UI::UIManager>().front()) && !registry.any_of<UI::PauseScreen>(registry.view<UI::UIManager>().front()))
			registry.patch<GAME::GameManager>(registry.view<GAME::GameManager>().front());

		registry.patch<AI::AIDirector>(registry.view<AI::AIDirector>().front());
		registry.patch<GAME::StateManager>(registry.view<GAME::StateManager>().front());
		registry.patch<AUDIO::MusicHandle>(registry.view<AUDIO::MusicHandle>().front());
		registry.patch<GAME::PickupManager>(registry.view<GAME::PickupManager>().front());
		for (auto entity : winView)
		{
			if (registry.any_of<APP::WindowClosed>(entity)) winCloseFound = true;
			else registry.patch<APP::Window>(entity);
			if (registry.any_of<APP::WindowClosed>(entity)) winCloseFound = true;
			if (winCloseFound) ++closedCount;
		}

		APP::CleanUpWindows(registry);

	} while (winView.size() != closedCount);
}
