#include "../CCL.h"
#include "Font.h"
#include "BLIT_Font.h"
#include "../DRAW/DrawComponents.h"
#include "../APP/Window.hpp"
#include "../GAME/GameComponents.h"
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif
#include "Overlay.h"
#include "UIComponents.h"

namespace UI
{
	void Construct_UI(entt::registry& registry, entt::entity entity)
	{
		auto& win = registry.get<GW::SYSTEM::GWindow>(entity);
		auto& window = registry.get<APP::Window>(entity);
		auto& ui = registry.get<UIManager>(entity);

		ui.overlay = new Overlay(window.width, window.height, win, registry.get<DRAW::VulkanRenderer>(entity).vlkSurface, 0);
		ui.blitter = new GW::GRAPHICS::GBlitter();
		ui.blitter->Create(window.width, window.height);
		ui.font = new BLIT_Font(*ui.blitter, "../font.tga", font_Arial);
	}

	void Update_UI(entt::registry& registry, entt::entity entity)
	{
		auto& window = registry.get<APP::Window>(entity);
		auto& ui = registry.get<UIManager>(entity);
		
		ui.blitter->ClearColor(0x0);
		
		auto& scoreComponent = registry.get<GAME::Score>(registry.view<GAME::Score>().front());
		std::string scoreText = "Score " + std::to_string(scoreComponent.score);
		ui.font->DrawTextImmediate(0, 24, scoreText.c_str(), scoreText.size());

		std::string highScoreText = "High Score " + std::to_string(scoreComponent.highScore);
		ui.font->DrawTextImmediate(0, 48, highScoreText.c_str(), highScoreText.size());
		
		unsigned int* color_pix;
		ui.overlay->LockForUpdate(window.width * window.height, &color_pix);
		ui.blitter->ExportResult(false, window.width, window.height, 0, 0, color_pix, nullptr, nullptr);
		
		//Edit pixels here
		//for (int i = 0; i < window.width * window.height; i++)
		//{
		//	color_pix[i] = 0xFF7000000;
		//}
		
		ui.overlay->Unlock();
		ui.overlay->TransferOverlay();
		ui.overlay->RenderOverlay();
	}

	void Destroy_UI(entt::registry& registry, entt::entity entity)
	{
		auto& ui = registry.get<UIManager>(entity);

		delete ui.blitter;
		delete ui.font;
		delete ui.overlay;
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<UIManager>().connect<Construct_UI>();
		registry.on_update<UIManager>().connect<Update_UI>();
		registry.on_destroy<UIManager>().connect<Destroy_UI>();
	}
}